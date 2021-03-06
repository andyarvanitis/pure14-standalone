-----------------------------------------------------------------------------
--
-- Module      :  Language.PureScript
-- Copyright   :  (c) 2013-15 Phil Freeman, (c) 2014 Gary Burgess, and other contributors
-- License     :  MIT
--
-- Maintainer  :  Andy Arvanitis
-- Stability   :  experimental
-- Portability :
--
-- |
-- The main compiler module
--
-----------------------------------------------------------------------------

{-# LANGUAGE DataKinds #-}
{-# LANGUAGE QuasiQuotes #-}
{-# LANGUAGE TemplateHaskell #-}
{-# LANGUAGE FlexibleContexts #-}
{-# LANGUAGE ScopedTypeVariables #-}

module Language.PureScript.Cpp
  ( module P
  , compile
  , compile'
  , RebuildPolicy(..)
  , MonadMake(..)
  , make
  , version
  ) where

import Data.FileEmbed (embedFile)
import Data.Function (on)
import Data.List (sortBy, groupBy, intercalate)
import Data.Maybe (fromMaybe)
import Data.Time.Clock
import Data.Version (Version)
import qualified Data.Traversable as T (traverse)
import qualified Data.ByteString.UTF8 as BU
import qualified Data.Map as M
import qualified Data.Set as S

import Control.Applicative
import Control.Arrow ((&&&))
import Control.Monad.Error.Class (MonadError(..))
import Control.Monad.Reader
import Control.Monad.Writer

import System.FilePath ((</>))

import Language.PureScript.AST as P
import Language.PureScript.Comments as P
import Language.PureScript.CodeGen as P
import Language.PureScript.CodeGen.Cpp as P
import Language.PureScript.DeadCodeElimination as P
import Language.PureScript.Environment as P
import Language.PureScript.Errors as P
import Language.PureScript.Kinds as P
import Language.PureScript.Linter as P
import Language.PureScript.ModuleDependencies as P
import Language.PureScript.Names as P
import Language.PureScript.Options as P
import Language.PureScript.Parser as P
import Language.PureScript.Pretty as P
import Language.PureScript.Pretty.Cpp as P
import Language.PureScript.Renamer as P
import Language.PureScript.Sugar as P
import Control.Monad.Supply as P
import Language.PureScript.TypeChecker as P
import Language.PureScript.Types as P
import qualified Language.PureScript.Core as CR
import qualified Language.PureScript.CoreFn as CF
import qualified Language.PureScript.CoreImp as CI
import qualified Language.PureScript.Constants as C

import qualified Paths_pure14 as Paths

-- |
-- Compile a collection of modules
--
-- The compilation pipeline proceeds as follows:
--
--  * Sort the modules based on module dependencies, checking for cyclic dependencies.
--
--  * Perform a set of desugaring passes.
--
--  * Type check, and elaborate values to include type annotations and type class dictionaries.
--
--  * Regroup values to take into account new value dependencies introduced by elaboration.
--
--  * Eliminate dead code.
--
--  * Generate C++11, and perform optimization passes.
--
--  * Pretty-print the generated C++11
--
compile :: (Functor m, Applicative m, MonadError MultipleErrors m, MonadWriter MultipleErrors m, MonadReader (Options Compile) m)
        => [Module] -> [String] -> m (String, String, Environment)
compile = compile' initEnvironment

compile' :: (Functor m, Applicative m, MonadError MultipleErrors m, MonadWriter MultipleErrors m, MonadReader (Options Compile) m)
         => Environment -> [Module] -> [String] -> m (String, String, Environment)
compile' env ms prefix = do
  noPrelude <- asks optionsNoPrelude
  unless noPrelude (checkPreludeIsDefined ms)
  additional <- asks optionsAdditional
  mainModuleIdent <- asks (fmap moduleNameFromString . optionsMain)
  (sorted, _) <- sortModules $ map importPrim $ if noPrelude then ms else map importPrelude ms
  mapM_ lint sorted
  (desugared, nextVar) <- runSupplyT 0 $ desugar sorted
  (elaborated, env') <- runCheck' env $ forM desugared $ typeCheckModule mainModuleIdent
  regrouped <- createBindingGroupsModule . collapseBindingGroupsModule $ elaborated
  let corefn = map (CF.moduleToCoreFn env') regrouped
      entryPoints = moduleNameFromString `map` entryPointModules additional
      elim = if null entryPoints then corefn else eliminateDeadCode entryPoints corefn
      renamed = renameInModules elim
      codeGenModuleNames = moduleNameFromString `map` codeGenModules additional
      modulesToCodeGen = if null codeGenModuleNames then renamed else filter (\(CR.Module _ mn _ _ _ _) -> mn `elem` codeGenModuleNames) renamed
  cpp <- concat <$> evalSupplyT nextVar (T.traverse (CI.moduleToCoreImp >=> moduleToCpp env') modulesToCodeGen)
  let exts = intercalate "\n" . map (`moduleToPs` env') $ regrouped
  cpp' <- generateMain env' cpp
  let pcpp = unlines $ map ("// " ++) prefix ++ [prettyPrintCpp cpp']
  return (pcpp, exts, env')

generateMain :: (MonadError MultipleErrors m, MonadReader (Options Compile) m) => Environment -> [Cpp] -> m [Cpp]
generateMain env cpp = do
  main <- asks optionsMain
  additional <- asks optionsAdditional
  case moduleNameFromString <$> main of
    Just mmi -> do
      when ((mmi, Ident C.main) `M.notMember` names env) $
        throwError . errorMessage $ NameIsUndefined (Ident C.main)
      return $ cpp ++ [CppApp (CppAccessor Nothing C.main (CppAccessor Nothing (moduleNameToCpp mmi) (CppVar (browserNamespace additional)))) []]
    _ -> return cpp

-- |
-- A type class which collects the IO actions we need to be able to run in "make" mode
--
class (MonadReader (P.Options P.Make) m, MonadError MultipleErrors m, MonadWriter MultipleErrors m) => MonadMake m where
  -- |
  -- Get a file timestamp
  --
  getTimestamp :: FilePath -> m (Maybe UTCTime)

  -- |
  -- Read a file as a string
  --
  readTextFile :: FilePath -> m String

  -- |
  -- Write a text file
  --
  writeTextFile :: FilePath -> String -> m ()

  -- |
  -- Respond to a progress update
  --
  progress :: String -> m ()

-- |
-- Determines when to rebuild a module
--
data RebuildPolicy
  -- | Never rebuild this module
  = RebuildNever
  -- | Always rebuild this module
  | RebuildAlways deriving (Show, Eq, Ord)

-- Traverse (Either e) instance (base 4.7)
traverseEither :: Applicative f => (a -> f b) -> Either e a -> f (Either e b)
traverseEither _ (Left x) = pure (Left x)
traverseEither f (Right y) = Right <$> f y

-- |
-- Compiles in "make" mode, compiling each module separately to a Cpp files and an externs file
--
-- If timestamps have not changed, the externs file can be used to provide the module's types without
-- having to typecheck the module again.
--
make :: forall m. (Functor m, Applicative m, Monad m, MonadMake m)
     => FilePath -> [(Either RebuildPolicy FilePath, Module)] -> [String] -> m Environment
make outputDir ms prefix = do
  noPrelude <- asks optionsNoPrelude
  unless noPrelude (checkPreludeIsDefined (map snd ms))
  let filePathMap = M.fromList (map (\(fp, Module _ mn _ _) -> (mn, fp)) ms)

  (sorted, graph) <- sortModules $ map importPrim $ if noPrelude then map snd ms else map (importPrelude . snd) ms

  mapM_ lint sorted

  toRebuild <- foldM (\s (Module _ moduleName' _ _) -> do
    let filePath = runModuleName moduleName'

        fileBase = outputDir </> filePath </> (last . words . dotsTo ' ' $ runModuleName moduleName')
        srcFile = fileBase ++ ".cc"
        externsFile = outputDir </> filePath </> "externs.purs"
        inputFile = fromMaybe (error "Module has no filename in 'make'") $ M.lookup moduleName' filePathMap

    cppTimestamp <- getTimestamp srcFile
    externsTimestamp <- getTimestamp externsFile
    inputTimestamp <- traverseEither getTimestamp inputFile

    return $ case (inputTimestamp, cppTimestamp, externsTimestamp) of
      (Right (Just t1), Just t2, Just t3) | t1 < min t2 t3 -> s
      (Left RebuildNever, Just _, Just _) -> s
      _ -> S.insert moduleName' s) S.empty sorted

  marked <- rebuildIfNecessary (reverseDependencies graph) toRebuild sorted

  when (any fst marked) $ do -- TODO: it should only be updated if any files have been added/removed
    writeTextFile (outputDir </> "CMakeLists.txt") cmakeListsTxt
    writeTextFile (outputDir </> "PureScript/any_map.hh")     $ BU.toString $(embedFile "include/any_map.hh")
    writeTextFile (outputDir </> "PureScript/bind.hh")        $ BU.toString $(embedFile "include/bind.hh")
    writeTextFile (outputDir </> "PureScript/memory.hh")      $ BU.toString $(embedFile "include/memory.hh")
    writeTextFile (outputDir </> "PureScript/PureScript.hh")  $ BU.toString $(embedFile "include/purescript.hh")
    writeTextFile (outputDir </> "PureScript/shared_list.hh") $ BU.toString $(embedFile "include/shared_list.hh")
    -- TODO: temporary
    writeTextFile (outputDir </> "PureScript/prelude_ffi.hh") $ BU.toString $(embedFile "include/prelude_ffi.hh")

  (desugared, nextVar) <- runSupplyT 0 $ zip (map fst marked) <$> desugar (map snd marked)

  evalSupplyT nextVar $ go initEnvironment desugared

  where
  go :: Environment -> [(Bool, Module)] -> SupplyT m Environment
  go env [] = return env
  go env ((False, m) : ms') = do
    (_, env') <- lift . runCheck' env $ typeCheckModule Nothing m

    go env' ms'
  go env ((True, m@(Module coms moduleName' _ exps)) : ms') = do
    let filePath = dotsTo '/' $ runModuleName moduleName'
        fileBase = outputDir </> filePath </> (last . words . dotsTo ' ' $ runModuleName moduleName')
        srcFile = fileBase ++ ".cc"
        headerFile = fileBase ++ ".hh"
        externsFile = outputDir </> filePath </> "externs.purs"

    lift . progress $ "Compiling " ++ runModuleName moduleName'

    (Module _ _ elaborated _, env') <- lift . runCheck' env $ typeCheckModule Nothing m

    regrouped <- createBindingGroups moduleName' . collapseBindingGroups $ elaborated

    let mod' = Module coms moduleName' regrouped exps
    let corefn = CF.moduleToCoreFn env' mod'
    let [renamed] = renameInModules [corefn]

    cpps <- (CI.moduleToCoreImp >=> moduleToCpp env') renamed
    let (hdrs,srcs) = span (/= CppEndOfHeader) cpps

    psrcs <- prettyPrintCpp <$> pure srcs
    phdrs <- prettyPrintCpp <$> pure hdrs

    let src = unlines $ map ("// " ++) prefix ++ [psrcs]
    let hdr = unlines $ map ("// " ++) prefix ++ [phdrs]
    let exts = unlines $ map ("-- " ++) prefix ++ [moduleToPs mod' env']

    lift $ writeTextFile srcFile src
    lift $ writeTextFile headerFile hdr
    lift $ writeTextFile externsFile exts

    go env' ms'

  rebuildIfNecessary :: M.Map ModuleName [ModuleName] -> S.Set ModuleName -> [Module] -> m [(Bool, Module)]
  rebuildIfNecessary _ _ [] = return []
  rebuildIfNecessary graph toRebuild (m@(Module _ moduleName' _ _) : ms') | moduleName' `S.member` toRebuild = do
    let deps = fromMaybe [] $ moduleName' `M.lookup` graph
        toRebuild' = toRebuild `S.union` S.fromList deps
    (:) (True, m) <$> rebuildIfNecessary graph toRebuild' ms'
  rebuildIfNecessary graph toRebuild (Module _ moduleName' _ _ : ms') = do
    let externsFile = outputDir </> (dotsTo '/' $ runModuleName moduleName') </> "externs.purs"
    externs <- readTextFile externsFile
    externsModules <- fmap (map snd) . either (throwError . errorMessage . ErrorParsingExterns) return $ P.parseModulesFromFiles id [(externsFile, externs)]
    case externsModules of
      [m'@(Module _ moduleName'' _ _)] | moduleName'' == moduleName' -> (:) (False, m') <$> rebuildIfNecessary graph toRebuild ms'
      _ -> throwError . errorMessage . InvalidExternsFile $ externsFile

checkPreludeIsDefined :: (MonadWriter MultipleErrors m) => [Module] -> m ()
checkPreludeIsDefined ms = do
  let mns = map getModuleName ms
  unless (preludeModuleName `elem` mns) $
    tell (errorMessage PreludeNotPresent)

reverseDependencies :: ModuleGraph -> M.Map ModuleName [ModuleName]
reverseDependencies g = combine [ (dep, mn) | (mn, deps) <- g, dep <- deps ]
  where
  combine :: (Ord a) => [(a, b)] -> M.Map a [b]
  combine = M.fromList . map ((fst . head) &&& map snd) . groupBy ((==) `on` fst) . sortBy (compare `on` fst)

-- |
-- Add an import declaration for a module if it does not already explicitly import it.
--
addDefaultImport :: ModuleName -> Module -> Module
addDefaultImport toImport m@(Module coms mn decls exps)  =
  if isExistingImport `any` decls || mn == toImport then m
  else Module coms mn (ImportDeclaration toImport Implicit Nothing : decls) exps
  where
  isExistingImport (ImportDeclaration mn' _ _) | mn' == toImport = True
  isExistingImport (PositionedDeclaration _ _ d) = isExistingImport d
  isExistingImport _ = False

importPrim :: Module -> Module
importPrim = addDefaultImport (ModuleName [ProperName C.prim])

preludeModuleName :: ModuleName
preludeModuleName = ModuleName [ProperName C.prelude]

importPrelude :: Module -> Module
importPrelude = addDefaultImport preludeModuleName

version :: Version
version = Paths.version

-- TODO: quick and dirty for now -- explicit file list would be better
cmakeListsTxt :: String
cmakeListsTxt = intercalate "\n" lines'
  where lines' = [ "cmake_minimum_required (VERSION 3.0)"
                 , "project (Main)"
                 , "file (GLOB_RECURSE SRCS *.cc)"
                 , "file (GLOB_RECURSE HDRS *.hh)"
                 , "add_executable (Main ${SRCS} ${HDRS})"
                 , "include_directories (${CMAKE_CURRENT_SOURCE_DIR})"
                 , "set (CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} \"-std=c++14\")"
                 ]
