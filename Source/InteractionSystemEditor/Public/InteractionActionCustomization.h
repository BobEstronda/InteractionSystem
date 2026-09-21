#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SComboBox.h"

class IPropertyHandle;
class AActor;

enum class EInteractionFunctionListMode : uint8
{
	// FInteractionOutputAction: BlueprintCallable/Event
	CallableFunctions,

	// FInteractionInputAction: multicast delegate BlueprintAssignable
	AssignableDelegates
};


class FInteractionActionCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	//~ End IPropertyTypeCustomization

private:
	TSharedPtr<IPropertyHandle> TargetActorHandle;
	TSharedPtr<IPropertyHandle> FunctionNameHandle;

	TSharedPtr<SComboBox<TSharedPtr<FName>>> FunctionComboBox;
	TArray<TSharedPtr<FName>> FunctionOptions;

	EInteractionFunctionListMode ListMode = EInteractionFunctionListMode::CallableFunctions;

	static EInteractionFunctionListMode DetermineListMode(const TSharedRef<IPropertyHandle>& StructPropertyHandle);

	AActor* ResolveTargetActor() const;

	void RebuildFunctionOptions();

	void OnTargetActorChanged();

	TSharedRef<SWidget> OnGenerateFunctionWidget(TSharedPtr<FName> InItem) const;
	void OnFunctionSelected(TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectInfo);
	FText GetCurrentFunctionText() const;
	bool IsFunctionComboEnabled() const;
};
