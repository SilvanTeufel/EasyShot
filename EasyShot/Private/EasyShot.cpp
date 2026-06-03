// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "EasyShot.h"

#define LOCTEXT_NAMESPACE "FEasyShotModule"

void FEasyShotModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FEasyShotModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEasyShotModule, EasyShot)