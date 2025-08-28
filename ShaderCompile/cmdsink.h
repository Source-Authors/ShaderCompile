//====== Copyright � 1996-2006, Valve Corporation, All rights reserved. =======//
//
// Purpose: Command sink interface implementation.
//
// $NoKeywords: $
//
//=============================================================================//

#pragma once

#include <cstddef>

namespace CmdSink
{

/*

struct IResponse

Interface to give back command execution results.

*/
class IResponse
{
public:
	virtual ~IResponse() = default;

	// Returns whether the command succeeded
	virtual bool Succeeded() const = 0;

	// If the command succeeded returns the result buffer length, otherwise zero
	virtual size_t GetResultBufferLen() const = 0;
	// If the command succeeded returns the result buffer base pointer, otherwise NULL
	virtual const void* GetResultBuffer() const = 0;

	// Returns a zero-terminated string of messages reported during command execution, or NULL if nothing was reported
	virtual const char* GetListing() const = 0;
};

}; // namespace CmdSink