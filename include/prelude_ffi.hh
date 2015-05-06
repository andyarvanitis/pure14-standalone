#ifndef PreludeFFI_HH
#define PreludeFFI_HH

// These are just temporary Prelude FFI stubs. This file will go away after
// the PureScript FFI is refactored to handle non-JS FFIs.

#include "purescript.hh"

namespace Prelude {

    using namespace PureScript;

	inline auto showStringImpl(string s) -> string {
		return s;
	}

	inline auto showNumberImpl(double x) -> string {
		return std::to_string(x);
	}

	inline auto showIntImpl(long x) -> string {
		return std::to_string(x);
	}

	inline auto showCharImpl(char x) -> string {
		return std::to_string(x);
	}

    template <typename A>
	inline auto showArrayImpl(fn<A,list<A>> f) -> fn<list<A>,string> {
		return [=](list<A> a) {
		    return "";
		};
	}

	inline auto numAdd(double x) -> fn<double,double> {
		return [=](double y) {
			return x + y;
		};
	}

	inline auto numSub(double x) -> fn<double,double> {
		return [=](double y) {
			return x - y;
		};
	}

	inline auto numMul(double x) -> fn<double,double> {
		return [=](double y) {
			return x * y;
		};
	}

	inline auto numDiv(double x) -> fn<double,double> {
		return [=](double y) {
			return x / y;
		};
	}

	inline auto intAdd(long x) -> fn<long,long> {
		return [=](long y) {
			return x + y;
		};
	}

	inline auto intSub(long x) -> fn<long,long> {
		return [=](long y) {
			return x - y;
		};
	}

	inline auto intMul(long x) -> fn<long,long> {
		return [=](long y) {
			return x * y;
		};
	}

	inline auto intDiv(long x) -> fn<long,long> {
		return [=](long y) {
			return x / y;
		};
	}

	inline auto intMod(long x) -> fn<long,long> {
		return [=](long y) {
			return x % y;
		};
	}

	inline auto concatString(string x) -> fn<string,string> {
		return [=](string y) {
			return x + y;
		};
	}

	template <typename T>
	inline auto refEq(T ref1) -> fn<T,bool> {
		return [=](T ref2) {
			return ref1 == ref2;
		};
	}

    template <typename A>
	inline auto eqArrayImpl(fn<A,fn<A,bool>> f) -> fn<list<A>,fn<list<A>,bool>> {
        return [=](list<A> xs) {
            return [=](list<A> ys) {
                return false;
            };
		};
	}

	inline auto boolAnd(bool x) -> fn<bool,bool> {
		return [=](bool y) {
			return x && y;
		};
	}

	inline auto boolOr(bool x) -> fn<bool,bool> {
		return [=](bool y) {
			return x || y;
		};
	}

	inline auto boolNot(bool x) -> bool {
		return !x;
	}

    template <typename A>
    inline auto unsafeCompareImpl(A arg) -> fn<A,std::shared_ptr<void>> {
        // return unsafeCompareImpl<A><A>(construct<LT_>())(construct<EQ_>())(construct<GT_>())(arg);
		return [=](A) {
			return nullptr;
		};
    }

    template <typename A>
    inline auto concatArray(list<A> a) -> fn<list<A>,list<A>> {
        return [=](list<A> b) {
            return a.append(b);
        };
    }

    template <typename A, typename B>
    inline auto arrayMap(fn<A,B> f) -> fn<list<A>,list<B>> {
        return [=](list<A> a) {
            return list<B>();
        };
    }

    template <typename A, typename B>
    inline auto arrayBind(list<A> a) -> fn<fn<A,list<B>>,list<B>> {
        return [=](fn<A,list<B>> f) {
            return list<B>();
        };
    }
}


#endif // PreludeFFI_HH
