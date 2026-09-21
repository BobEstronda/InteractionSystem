// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractionSystemEditor.h"

#define LOCTEXT_NAMESPACE "FInteractionSystemEditorModule"

void FInteractionSystemEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	const TArray<FName> StructsToCustomize =
	{
		TEXT("InteractionOutputAction"),
		TEXT("InteractionInputAction")
	};

	for (const FName& StructName : StructsToCustomize)
	{
		PropertyModule.RegisterCustomPropertyTypeLayout(
			StructName,
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInteractionActionCustomization::MakeInstance)
		);
		RegisteredStructCustomizations.Add(StructName);
	}

	PropertyModule.NotifyCustomizationModuleChanged();
}

void FInteractionSystemEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule =
			FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		for (const FName& StructName : RegisteredStructCustomizations)
		{
			PropertyModule.UnregisterCustomPropertyTypeLayout(StructName);
		}

		PropertyModule.NotifyCustomizationModuleChanged();
	}

	RegisteredStructCustomizations.Empty();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FInteractionSystemEditorModule, InteractionSystemEditor)