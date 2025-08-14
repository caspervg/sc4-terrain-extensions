/*
 * This file is part of sc4-bulldoze-extensions, a DLL Plugin for
 * SimCity 4 extends the bulldoze tool.
 *
 * Copyright (C) 2024, 2025 Nicholas Hayes
 *
 * sc4-bulldoze-extensions is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * sc4-bulldoze-extensions is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with SC4ClearPollution.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "Patcher.h"
#include <Windows.h>
#include "wil/resource.h"
#include "wil/win32_helpers.h"
#include <stdexcept>

void Patcher::InstallJump(uintptr_t address, uintptr_t destination)
{
	DWORD oldProtect;
	THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(reinterpret_cast<void*>(address), 5, PAGE_EXECUTE_READWRITE, &oldProtect));

	*((uint8_t*)address) = 0xE9;
	*((uintptr_t*)(address + 1)) = destination - address - 5;
}

void Patcher::InstallJumpTableHook(uintptr_t targetAddress, uintptr_t newValue)
{
	// Allow the executable memory to be written to.
	DWORD oldProtect = 0;
	THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(
		reinterpret_cast<LPVOID>(targetAddress),
		sizeof(uintptr_t),
		PAGE_EXECUTE_READWRITE,
		&oldProtect));

	// Patch the memory at the specified address.
	*reinterpret_cast<uintptr_t*>(targetAddress) = newValue;
}

uintptr_t Patcher::InstallJumpHook(uintptr_t address, uintptr_t destination, size_t stolenBytes)
{
	if (stolenBytes < 5)
		throw std::runtime_error("Not enough bytes to install jump");

	// Allocate executable memory for the trampoline
	void* gateway = VirtualAlloc(nullptr, stolenBytes + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!gateway)
		throw std::runtime_error("Failed to allocate trampoline");

	// Copy original bytes into the trampoline
	memcpy(gateway, reinterpret_cast<void*>(address), stolenBytes);

	// Add a jump from the trampoline back to the original function after stolen bytes
	uintptr_t gatewayAddr = reinterpret_cast<uintptr_t>(gateway);
	uintptr_t jumpBackAddr = (address + stolenBytes) - (gatewayAddr + stolenBytes) - 5;
	*reinterpret_cast<uint8_t*>(gatewayAddr + stolenBytes) = 0xE9;
	*reinterpret_cast<uintptr_t*>(gatewayAddr + stolenBytes + 1) = jumpBackAddr;

	// Overwrite original with jump to destination
	DWORD oldProtect;
	THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(reinterpret_cast<void*>(address), stolenBytes, PAGE_EXECUTE_READWRITE, &oldProtect));

	uintptr_t relativeAddress = destination - address - 5;
	*reinterpret_cast<uint8_t*>(address) = 0xE9;
	*reinterpret_cast<uintptr_t*>(address + 1) = relativeAddress;

	// Fill any remaining bytes with NOPs
	for (size_t i = 5; i < stolenBytes; i++)
	{
		*reinterpret_cast<uint8_t*>(address + i) = 0x90;
	}

	// Restore protection
	VirtualProtect(reinterpret_cast<void*>(address), stolenBytes, oldProtect, &oldProtect);

	return gatewayAddr;
}

void Patcher::InstallCallHook(uintptr_t address, void(*pfnFunc)(void))
{
	InstallCallHook(address, reinterpret_cast<uintptr_t>(pfnFunc));
}

void Patcher::InstallCallHook(uintptr_t address, uintptr_t pfnFunc)
{
	DWORD oldProtect;
	THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(reinterpret_cast<void*>(address), 5, PAGE_EXECUTE_READWRITE, &oldProtect));

	*((uint8_t*)address) = 0xE8;
	*((uintptr_t*)(address + 1)) = pfnFunc - address - 5;
}

void Patcher::OverwriteMemory(uintptr_t address, uint8_t newValue)
{
	DWORD oldProtect;
	// Allow the executable memory to be written to.
	THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(
		reinterpret_cast<void*>(address),
		sizeof(newValue),
		PAGE_EXECUTE_READWRITE,
		&oldProtect));

	// Patch the memory at the specified address.
	*((uint8_t*)address) = newValue;
}
