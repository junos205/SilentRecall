/*
* Copyright (c) 2025 Side Effects Software Inc.  All rights reserved.
*
* Redistribution and use of in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
* promote products derived from this software without specific prior
* written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "SidefxLabsEditor.h"
#include "HoudiniCreateNewVatWindow.h"
#include "HoudiniVatPropertiesCustomization.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FSidefxLabsEditorModule"

/**
 * Helper class to manage menu registration for SideFX Labs plugin.
 */
class FMenuManager
{
public:
	/** Registers the main SideFX Labs menu in the level editor. */
	static void RegisterSidefxLabsMenu();
	
	/** Registers the SideFX Labs submenu with all menu items. */
	static void RegisterSidefxLabsSubMenu();

private:
	/** Registers the Help and Support section of the submenu. */
	static void RegisterHelpAndSupportSection(UToolMenu* SidefxLabsEditorSubMenu);
};

/**
 * Helper class to manage property customizations for SideFX Labs.
 */
class FPropertyCustomizationManager
{
public:
	/** Initializes all property customizations. */
	static void Initialize();
	
	/** Unregisters all property customizations. */
	static void Shutdown();

private:
	/** Registers Houdini details category in the property editor. */
	static void RegisterHoudiniDetailsCategory();
	
	/** Registers custom class layouts for VAT properties. */
	static void RegisterCustomizations();
};

void FMenuManager::RegisterSidefxLabsMenu()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus)
	{
		return;
	}

	UToolMenu* MainMenu = ToolMenus->ExtendMenu("LevelEditor.MainMenu");
	if (!MainMenu)
	{
		return;
	}

	FToolMenuSection& PluginsSection = MainMenu->AddSection(
		"SideFXLabs",
		LOCTEXT("SideFXLabs", "SideFX Labs")
	);
	
	PluginsSection.AddSubMenu(
		"SidefxLabsEditor_SubMenu",
		LOCTEXT("SidefxLabsEditor_SubMenu", "SideFX Labs"),
		LOCTEXT("SidefxLabsEditor_SubMenu_ToolTip", "Open the SideFX Labs menu"),
		FNewToolMenuChoice(),
		false,
		FSlateIcon("EditorStyle", "LevelEditor.Tabs.Tools")
	);

	RegisterSidefxLabsSubMenu();
}

void FMenuManager::RegisterSidefxLabsSubMenu()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus)
	{
		return;
	}

	UToolMenu* SidefxLabsEditorSubMenu = ToolMenus->ExtendMenu("LevelEditor.MainMenu.SidefxLabsEditor_SubMenu");
	if (!SidefxLabsEditorSubMenu)
	{
		return;
	}

	// Vertex Animation section
	FToolMenuSection& VertexAnimationSection = SidefxLabsEditorSubMenu->AddSection(
		"VertexAnimation",
		LOCTEXT("VertexAnimation_Heading", "Vertex Animation")
	);

	VertexAnimationSection.AddMenuEntry(
		"CreateNewVat",
		LOCTEXT("CreateNewVat", "Create New VAT"),
		LOCTEXT("CreateNewVat_ToolTip", "Create a new Vertex Animation Texture (VAT) asset"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FSidefxLabsEditorModule::CreateNewVat))
	);

	RegisterHelpAndSupportSection(SidefxLabsEditorSubMenu);
}

void FMenuManager::RegisterHelpAndSupportSection(UToolMenu* SidefxLabsEditorSubMenu)
{
	if (!SidefxLabsEditorSubMenu)
	{
		return;
	}

	FToolMenuSection& HelpAndSupportSection = SidefxLabsEditorSubMenu->AddSection(
		"HelpAndSupport",
		LOCTEXT("HelpAndSupport_Heading", "Help and Support")
	);

	// Website
	HelpAndSupportSection.AddMenuEntry(
		"Website",
		LOCTEXT("Website", "Website"),
		LOCTEXT("Website_ToolTip", "SideFX Labs website"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FPlatformProcess::LaunchURL(TEXT("https://www.sidefx.com/products/sidefx-labs/"), nullptr, nullptr);
		}))
	);

	// Documentation
	HelpAndSupportSection.AddMenuEntry(
		"Documentation",
		LOCTEXT("Documentation", "Documentation"),
		LOCTEXT("Documentation_ToolTip", "SideFX Labs documentation"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FPlatformProcess::LaunchURL(TEXT("https://www.sidefx.com/docs/houdini/labs/"), nullptr, nullptr);
		}))
	);

	// GitHub
	HelpAndSupportSection.AddMenuEntry(
		"GitHub",
		LOCTEXT("GitHub", "GitHub"),
		LOCTEXT("GitHub_ToolTip", "SideFX Labs GitHub repository"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FPlatformProcess::LaunchURL(TEXT("https://github.com/sideeffects/SidefxLabs"), nullptr, nullptr);
		}))
	);

	// ArtStation
	HelpAndSupportSection.AddMenuEntry(
		"ArtStation",
		LOCTEXT("ArtStation", "ArtStation"),
		LOCTEXT("ArtStation_ToolTip", "SideFX Labs ArtStation gallery"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FPlatformProcess::LaunchURL(TEXT("https://www.artstation.com/SidefxLabs"), nullptr, nullptr);
		}))
	);
}

void FPropertyCustomizationManager::Initialize()
{
	RegisterHoudiniDetailsCategory();
	RegisterCustomizations();
}

void FPropertyCustomizationManager::Shutdown()
{
	if (!FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		return;
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.UnregisterCustomClassLayout("SidefxLabs");
	PropertyEditorModule.UnregisterCustomClassLayout("CreateNewVatProperties");
}

void FPropertyCustomizationManager::RegisterHoudiniDetailsCategory()
{
	static const FName PropertyEditorModuleName("PropertyEditor");
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditorModuleName);

	const TSharedRef<FPropertySection> HoudiniSection = PropertyEditorModule.FindOrCreateSection(
		"Object",
		"Houdini",
		LOCTEXT("Houdini", "Houdini")
	);
	
	HoudiniSection->AddCategory("Houdini VAT");
}

void FPropertyCustomizationManager::RegisterCustomizations()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyEditorModule.RegisterCustomClassLayout(
		"SidefxLabs",
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniCreateNewVatWindow::MakeInstance)
	);

	PropertyEditorModule.RegisterCustomClassLayout(
		"CreateNewVatProperties",
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniVatPropertiesCustomization::MakeInstance)
	);
}

void FSidefxLabsEditorModule::StartupModule()
{
	InitializeMenu();
	InitializePropertyCustomization();
	RegisterTabSpawners();
}

void FSidefxLabsEditorModule::ShutdownModule()
{
	FPropertyCustomizationManager::Shutdown();

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("CreateNewVATTab");
}

void FSidefxLabsEditorModule::InitializeMenu()
{
	FMenuManager::RegisterSidefxLabsMenu();
}

void FSidefxLabsEditorModule::InitializePropertyCustomization()
{
	FPropertyCustomizationManager::Initialize();
}

void FSidefxLabsEditorModule::RegisterTabSpawners()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		"CreateNewVATTab",
		FOnSpawnTab::CreateStatic(&FHoudiniCreateNewVatWindow::CreatePropertyEditorTab)
	)
	.SetDisplayName(LOCTEXT("CreateNewVATTabTitle", "Create New VAT"))
	.SetTooltipText(LOCTEXT("CreateNewVATTabTooltip", "Opens the VAT creation window"))
	.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FSidefxLabsEditorModule::CreateNewVat()
{
	FHoudiniCreateNewVatWindow::OpenPropertyEditorWindow();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSidefxLabsEditorModule, SidefxLabsEditor)
