#include "InteractionActionCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "InteractionActionCustomization"

TSharedRef<IPropertyTypeCustomization> FInteractionActionCustomization::MakeInstance()
{
	return MakeShared<FInteractionActionCustomization>();
}

EInteractionFunctionListMode FInteractionActionCustomization::DetermineListMode(const TSharedRef<IPropertyHandle>& StructPropertyHandle)
{
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(StructPropertyHandle->GetProperty()))
	{
		if (StructProperty->Struct && StructProperty->Struct->GetFName() == TEXT("InteractionInputAction"))
		{
			return EInteractionFunctionListMode::AssignableDelegates;
		}
	}

	// FInteractionOutputAction (e qualquer outra struct nova) cai aqui por padrão.
	return EInteractionFunctionListMode::CallableFunctions;
}

void FInteractionActionCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.f)
	[
		StructPropertyHandle->CreatePropertyValueWidget(false)
	];
}

void FInteractionActionCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TargetActorHandle.Reset();
	FunctionNameHandle.Reset();
	FunctionComboBox.Reset();

	ListMode = DetermineListMode(StructPropertyHandle);

	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		TSharedPtr<IPropertyHandle> ChildHandlePtr = StructPropertyHandle->GetChildHandle(Index);
		if (!ChildHandlePtr.IsValid())
		{
			continue;
		}

		const FProperty* Property = ChildHandlePtr->GetProperty();
		const FName PropertyName = Property ? Property->GetFName() : NAME_None;

		if (PropertyName == TEXT("TargetActor"))
		{
			TargetActorHandle = ChildHandlePtr;
		}
		else if (PropertyName == TEXT("FunctionName"))
		{
			FunctionNameHandle = ChildHandlePtr;
		}
	}

	if (TargetActorHandle.IsValid())
	{
		TargetActorHandle->SetOnPropertyValueChanged(
			FSimpleDelegate::CreateSP(this, &FInteractionActionCustomization::OnTargetActorChanged));
	}

	RebuildFunctionOptions();

	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		TSharedPtr<IPropertyHandle> ChildHandlePtr = StructPropertyHandle->GetChildHandle(Index);
		if (!ChildHandlePtr.IsValid())
		{
			continue;
		}

		TSharedRef<IPropertyHandle> ChildHandle = ChildHandlePtr.ToSharedRef();
		const FProperty* Property = ChildHandle->GetProperty();
		const FName PropertyName = Property ? Property->GetFName() : NAME_None;

		if (PropertyName == TEXT("FunctionName"))
		{
			ChildBuilder.AddCustomRow(LOCTEXT("FunctionNameRow", "Function Name"))
			.NameContent()
			[
				ChildHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth(250.f)
			[
				SAssignNew(FunctionComboBox, SComboBox<TSharedPtr<FName>>)
				.OptionsSource(&FunctionOptions)
				.IsEnabled(this, &FInteractionActionCustomization::IsFunctionComboEnabled)
				.OnGenerateWidget(this, &FInteractionActionCustomization::OnGenerateFunctionWidget)
				.OnSelectionChanged(this, &FInteractionActionCustomization::OnFunctionSelected)
				.OnComboBoxOpening(this, &FInteractionActionCustomization::RebuildFunctionOptions)
				[
					SNew(STextBlock)
					.Text(this, &FInteractionActionCustomization::GetCurrentFunctionText)
				]
			];
		}
		else
		{
			ChildBuilder.AddProperty(ChildHandle);
		}
	}
}

AActor* FInteractionActionCustomization::ResolveTargetActor() const
{
	if (!TargetActorHandle.IsValid())
	{
		return nullptr;
	}

	FString PathString;
	if (TargetActorHandle->GetValueAsFormattedString(PathString) != FPropertyAccess::Success)
	{
		return nullptr;
	}

	if (PathString.IsEmpty() || PathString == TEXT("None"))
	{
		return nullptr;
	}

	const FSoftObjectPath SoftPath(PathString);
	UObject* ResolvedObject = SoftPath.ResolveObject();
	if (!ResolvedObject)
	{
		ResolvedObject = SoftPath.TryLoad();
	}

	return Cast<AActor>(ResolvedObject);
}

void FInteractionActionCustomization::RebuildFunctionOptions()
{
	FunctionOptions.Reset();
	FunctionOptions.Add(MakeShared<FName>(NAME_None));

	if (AActor* Actor = ResolveTargetActor())
	{
		TArray<FName> Names;

		if (ListMode == EInteractionFunctionListMode::CallableFunctions)
		{
			for (TFieldIterator<UFunction> FuncIt(Actor->GetClass(), EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
			{
				UFunction* Function = *FuncIt;
				if (!Function)
				{
					continue;
				}

				const bool bIsCallable = Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintEvent);
				const bool bHasNoParams = (Function->NumParms == 0);

				if (bIsCallable && bHasNoParams)
				{
					Names.Add(Function->GetFName());
				}
			}
		}
		else // EInteractionFunctionListMode::AssignableDelegates
		{
			for (TFieldIterator<FMulticastDelegateProperty> PropIt(Actor->GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
			{
				FMulticastDelegateProperty* DelegateProperty = *PropIt;
				if (!DelegateProperty)
				{
					continue;
				}

				const bool bIsAssignable = DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable);
				const UFunction* SignatureFunction = DelegateProperty->SignatureFunction;
				//const bool bHasNoParams = SignatureFunction && (SignatureFunction->NumParms == 0);

				if (bIsAssignable /* && bHasNoParams */ )
				{
					Names.Add(DelegateProperty->GetFName());
				}
			}
		}

		Names.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });

		for (const FName& Name : Names)
		{
			FunctionOptions.Add(MakeShared<FName>(Name));
		}
	}

	if (FunctionComboBox.IsValid())
	{
		FunctionComboBox->RefreshOptions();
	}
}

void FInteractionActionCustomization::OnTargetActorChanged()
{
	AActor* Actor = ResolveTargetActor();

	if (!Actor && FunctionNameHandle.IsValid())
	{
		FunctionNameHandle->SetValue(NAME_None);
	}

	RebuildFunctionOptions();
}

TSharedRef<SWidget> FInteractionActionCustomization::OnGenerateFunctionWidget(TSharedPtr<FName> InItem) const
{
	return SNew(STextBlock)
		.Text(FText::FromName(InItem.IsValid() ? *InItem : NAME_None));
}

void FInteractionActionCustomization::OnFunctionSelected(TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (FunctionNameHandle.IsValid() && NewSelection.IsValid())
	{
		FunctionNameHandle->SetValue(*NewSelection);
	}
}

FText FInteractionActionCustomization::GetCurrentFunctionText() const
{
	if (!FunctionNameHandle.IsValid())
	{
		return FText::GetEmpty();
	}

	FName CurrentValue;
	if (FunctionNameHandle->GetValue(CurrentValue) == FPropertyAccess::Success)
	{
		return FText::FromName(CurrentValue);
	}

	return LOCTEXT("MultipleValues", "Múltiplos valores");
}

bool FInteractionActionCustomization::IsFunctionComboEnabled() const
{
	return ResolveTargetActor() != nullptr;
}

#undef LOCTEXT_NAMESPACE
