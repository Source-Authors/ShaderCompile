#pragma once

#include <new>
#include <type_traits>

#ifdef _WIN32
#include <new.h>
#endif

#include "basetypes.h"

#if __cpp_lib_to_underlying
using std::to_underlying;
#else
template <typename T>
[[nodiscard]] constexpr std::underlying_type_t<T> to_underlying( T value ) noexcept
{
	return static_cast<std::underlying_type_t<T>>( value );
}
#endif

#ifdef _WIN32
enum class NewMode
{
	Unknown = -1,
	DoNotCallNewHandlerOnMallocFailure = 0,
	CallNewHandlerOnMallocFailure = 1,
};

class ScopedNewMode
{
public:
	explicit ScopedNewMode( NewMode newMode ) noexcept
		: m_old_mode{ static_cast<NewMode>( _set_new_mode( to_underlying( newMode ) ) ) },
		  m_new_mode{ newMode }
	{
		// On Windows std::set_new_handler uses _set_new_handler internally.
		// 
		// When we use _set_new_mode(1) malloc failures will be send to _set_new_handler
		// so we call our new handler in the end.
	}
	~ScopedNewMode() noexcept
	{
		[[maybe_unused]] const auto previousMode = static_cast<NewMode>( _set_new_mode( to_underlying( m_old_mode ) ) );
		// Ensure nobody changed mode between.
		Assert( previousMode == m_new_mode );
	}

	ScopedNewMode( const ScopedNewMode& ) = delete;
	ScopedNewMode& operator=( const ScopedNewMode& ) = delete;
	ScopedNewMode( ScopedNewMode&& ) = delete;
	ScopedNewMode& operator=( ScopedNewMode&& ) = delete;

private:
	const NewMode m_old_mode, m_new_mode;
};
#endif

class ScopedNewHandler
{
public:
	explicit ScopedNewHandler( std::new_handler new_handler ) noexcept
		: m_old_handler{ std::set_new_handler( new_handler ) }
		, m_new_handler{ new_handler }
#ifdef _WIN32
		, m_scoped_new_mode{ NewMode::CallNewHandlerOnMallocFailure }
#endif
	{
		// On Windows std::set_new_handler uses _set_new_handler internally.
		// When we use _set_new_mode(1) malloc failures will be send to
		// _set_new_handler so we call our new handler.
	}
	~ScopedNewHandler() noexcept
	{
		[[maybe_unused]] const auto previousHandler = std::set_new_handler( m_old_handler );
		// Ensure nobody changed handler between.
		Assert( previousHandler == m_new_handler );
	}

	ScopedNewHandler( const ScopedNewHandler& ) = delete;
	ScopedNewHandler& operator=( const ScopedNewHandler& ) = delete;
	ScopedNewHandler( ScopedNewHandler&& ) = delete;
	ScopedNewHandler& operator=( ScopedNewHandler&& ) = delete;

private:
	const std::new_handler m_old_handler, m_new_handler;
#ifdef _WIN32
	const ScopedNewMode m_scoped_new_mode;
#endif
};
