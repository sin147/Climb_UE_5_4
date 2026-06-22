// Fill out your copyright notice in the Description page of Project Settings.


#include "ClimbComponent.h"
#include "ClimbData.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

// Sets default values for this component's properties
UClimbComponent::UClimbComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UClimbComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UClimbComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

// =========================================================================
// 核心函数实现
// =========================================================================

const FClimbInfo* UClimbComponent::GetCurrentClimbInfo() const
{
	if (!ClimbData)
	{
		return nullptr;
	}
	return ClimbData->GetClimbInfo(CurrentClimbType);
}

bool UClimbComponent::CanMoveTo(const FVector& InputValue) const
{
	// 未激活攀爬或没有有效类型时禁止移动
	if (!bClimbActive || CurrentClimbType == EClimbType::CT_None)
	{
		return false;
	}

	// 输入过小视为无效
	if (InputValue.IsNearlyZero())
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// 此处为基础预测实现，后续可根据 ClimbInfo.SlopeAngle 做更细致的检测
	// 例如：射线检测前方是否仍是符合当前 ClimbType 角度区间的可攀爬面。
	// 默认：仅在拥有当前 ClimbInfo 时允许移动。
	const FClimbInfo* Info = GetCurrentClimbInfo();
	if (!Info)
	{
		return false;
	}

	return true;
}

void UClimbComponent::Move(const FVector& InputValue)
{
	// 先预测下一个位置是否可达
	if (!CanMoveTo(InputValue))
	{
		// 不可移动时切回 Idle
		CurrentClimbState = EClimbState::CS_Idle;
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 应用一次基础位移（具体姿态/动画驱动由动画蓝图根据 CurrentClimbType + CurrentClimbState 处理）
	const FVector Delta = InputValue.GetSafeNormal() * MoveStep;
	Owner->AddActorWorldOffset(Delta, /*bSweep=*/true);

	// 切换为 Move 状态
	CurrentClimbState = EClimbState::CS_Move;
}

void UClimbComponent::ActiveClimb()
{
	if (bClimbActive)
	{
		return;
	}

	bClimbActive = true;
	CurrentClimbState = EClimbState::CS_Idle;

	// 若已经存在有效的攀爬类型，则在激活时触发一次 Enter
	if (CurrentClimbType != EClimbType::CT_None)
	{
		OnEnterClimbType(CurrentClimbType);
	}
}

void UClimbComponent::CancelClimb()
{
	if (!bClimbActive)
	{
		return;
	}

	// 如果当前有有效类型，需要先广播 Leave
	if (CurrentClimbType != EClimbType::CT_None)
	{
		OnLeaveClimbType(CurrentClimbType);
	}

	bClimbActive = false;
	CurrentClimbType = EClimbType::CT_None;
	CurrentClimbState = EClimbState::CS_Idle;
}

void UClimbComponent::SwitchClimbType(EClimbType NewClimbType)
{
	if (NewClimbType == CurrentClimbType)
	{
		return;
	}

	const EClimbType OldType = CurrentClimbType;

	// 离开旧类型
	if (OldType != EClimbType::CT_None)
	{
		OnLeaveClimbType(OldType);
	}

	CurrentClimbType = NewClimbType;
	CurrentClimbState = EClimbState::CS_Idle;

	// 进入新类型
	if (NewClimbType != EClimbType::CT_None)
	{
		OnEnterClimbType(NewClimbType);
	}
}

void UClimbComponent::OnEnterClimbType_Implementation(EClimbType ClimbType)
{
	// 基类默认不做处理，留给蓝图或子类扩展（例如：切换动画、播放特效等）
}

void UClimbComponent::OnLeaveClimbType_Implementation(EClimbType ClimbType)
{
	// 基类默认不做处理，留给蓝图或子类扩展
}

