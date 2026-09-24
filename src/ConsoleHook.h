#pragma once

namespace InventoryInjectorImproved::ConsoleHook
{
	/**
	 * Install the console-command hook once. The caller must already have reserved the
	 * trampoline budget in TrampolineBudget.h through a single SKSE::AllocTrampoline call.
	 * Safe to call repeatedly.
	 */
	void Install();
}
