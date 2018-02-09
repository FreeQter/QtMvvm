#ifndef QTMVVMCORE_HELPERTYPES_H
#define QTMVVMCORE_HELPERTYPES_H

#include <type_traits>
#include <tuple>
#include <functional>

#include <QtCore/qobject.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qvariant.h>

namespace QtMvvm {
namespace __helpertypes {

#if __cplusplus >= 201703L
template<typename... T>
using conjunction = std::conjunction<T...>;
#else
template<typename... T>
using conjunction = std::__and_<T...>;
#endif

static const QByteArray InjectablePrefix = QByteArrayLiteral("de.skycoder42.qtmvvm.injectable.");

template <typename T>
struct is_qobj : public std::is_base_of<QObject, T> {};

template <typename T>
struct is_qobj_ptr : public is_qobj<std::remove_pointer_t<T>> {};

template <typename TInterface, typename TService>
struct is_valid_interface : public conjunction<std::is_base_of<TInterface, TService>, is_qobj<TService>> {};

template <typename TInjectPtr>
inline QByteArray qobject_iid(std::enable_if_t<is_qobj_ptr<TInjectPtr>::value, void*> = nullptr) {
	return InjectablePrefix + std::remove_pointer_t<TInjectPtr>::staticMetaObject.className();
}

template <typename TInjectPtr>
inline QByteArray qobject_iid(std::enable_if_t<!is_qobj_ptr<TInjectPtr>::value, void*> = nullptr) {
	Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid injection type");
	return "";
}

template <typename TInjectPtr>
inline QByteArray inject_iid() {
	auto iid = qobject_interface_iid<TInjectPtr>();
	if(iid)
		return iid;
	else
		return qobject_iid<TInjectPtr>();
}

template <typename TFunc, typename T1, typename... TArgs>
inline Q_CONSTEXPR std::function<QObject*(QVariantList)> pack_function_imp(const TFunc &fn, QByteArrayList &injectables) {
	injectables.append(inject_iid<T1>());
	auto subFn = [fn](const QVariantList &params, int index, TArgs... tArgs) {
		--index;
		return fn(params, index, params[index].template value<T1>(), tArgs...);
	};
	return pack_function_imp<decltype(subFn), TArgs...>(subFn, injectables);
}

template <typename TFunc>
inline Q_CONSTEXPR std::function<QObject*(QVariantList)> pack_function_imp(const TFunc &fn, QByteArrayList &injectables) {
	Q_UNUSED(injectables)
	return [fn](const QVariantList &params) {
		return fn(params, params.size());
	};
}

template <typename T>
struct fn_info : public fn_info<decltype(&T::operator())> {};

template <typename TClass, typename TRet, typename... TArgs>
struct fn_info<TRet(TClass::*)(TArgs...) const>
{
	template <typename TFunctor>
	static inline Q_CONSTEXPR std::function<QObject*(QVariantList)> pack(const TFunctor &fn, QByteArrayList &injectables) {
		auto subFn = [fn](const QVariantList &params, int index, TArgs... args) -> QObject* {
			Q_UNUSED(params)
			Q_ASSERT_X(index == 0, Q_FUNC_INFO, "number of params does not equal recursion depth");
			return fn(args...);
		};
		return pack_function_imp<decltype(subFn), TArgs...>(subFn, injectables);
	}
};

template <typename TFunc>
inline Q_CONSTEXPR std::function<QObject*(QVariantList)> pack_function(const TFunc &fn, QByteArrayList &injectables) {
	return fn_info<TFunc>::pack(fn, injectables);
}


}
}

#endif // QTMVVMCORE_HELPERTYPES_H