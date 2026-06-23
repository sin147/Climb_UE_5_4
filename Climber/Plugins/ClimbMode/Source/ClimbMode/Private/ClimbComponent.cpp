// Fill out your copyright notice in the Description page of Project Settings.


#include "ClimbComponent.h"
#include "ClimbData.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

bool UClimbComponent::SweepLimbSphere(const FVector& LocalOffset, FHitResult& OutHit, FColor DebugColor) const
{
	OutHit = FHitResult();

	if (!OwnerCharacter)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 用 Actor 的世界变换把局部偏移变换到世界空间，使肢体跟随角色朝向
	const FTransform ActorTM = OwnerCharacter->GetActorTransform();
	const FVector Start = ActorTM.TransformPosition(LocalOffset);
	const FVector Forward = OwnerCharacter->GetActorForwardVector();
	const FVector End = Start + Forward * ClimbCheckDistance;

	const FName ProfileName("Pawn");
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbLimbSphereSweep), /*bTraceComplex=*/false);
	QueryParams.AddIgnoredActor(OwnerCharacter);

	const bool bHit = World->SweepSingleByProfile(OutHit, Start, End, FQuat::Identity, ProfileName, SphereShape, QueryParams);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		const bool bPersistent = false;
		const float LifeTime = DebugDrawDuration;

		// 起点球（指定颜色，浅色表现），终点球：未命中红、命中绿
		DrawDebugSphere(World, Start, SphereRadius, 12, DebugColor, bPersistent, LifeTime, 0, 1.f);
		DrawDebugSphere(World, End, SphereRadius, 12, bHit ? FColor::Green : FColor::Red, bPersistent, LifeTime, 0, 1.f);
		DrawDebugLine(World, Start, End, DebugColor, bPersistent, LifeTime, 0, 1.f);

		if (bHit)
		{
			DrawDebugPoint(World, OutHit.ImpactPoint, 8.f, FColor::Magenta, bPersistent, LifeTime);
			DrawDebugDirectionalArrow(World, OutHit.ImpactPoint,
									  OutHit.ImpactPoint + OutHit.ImpactNormal * 40.f,
									  6.f, FColor::Cyan, bPersistent, LifeTime, 0, 1.5f);
		}
	}
#endif

	return bHit;
}

bool UClimbComponent::SweepLeftHand(FHitResult& OutHit) const
{
	// 使用 Runtime 偏移，会被 MoveHand 交替修改
	return SweepLimbSphere(LeftHandRuntimeOffset, OutHit, FColor::Yellow);
}

bool UClimbComponent::SweepRightHand(FHitResult& OutHit) const
{
	return SweepLimbSphere(RightHandRuntimeOffset, OutHit, FColor::Orange);
}

void UClimbComponent::ResetHandRuntimeOffsets()
{
	LeftHandRuntimeOffset  = LeftHandOffset;
	RightHandRuntimeOffset = RightHandOffset;
	bMoveLeftHandNext = true;
}

void UClimbComponent::MoveHand(const FVector2D& InputValue)
{
	if (!OwnerCharacter)
	{
		return;
	}

	// 输入过小不动手，避免原地抽动
	if (InputValue.IsNearlyZero())
	{
		return;
	}

	// 1) 用当前墙面法线构造切平面基，把 2D 输入映射成 3D 世界方向
	//    （逻辑与 CanMoveTo 一致）
	const FVector N = CurrentSurfaceNormal.GetSafeNormal();

	FVector UpW = FVector::VectorPlaneProject(FVector::UpVector, N);
	if (UpW.IsNearlyZero())
	{
		UpW = FVector::VectorPlaneProject(OwnerCharacter->GetActorForwardVector(), N);
	}
	UpW = UpW.GetSafeNormal();
	const FVector RightW = FVector::CrossProduct(UpW, N).GetSafeNormal();

	const FVector WorldDir = (RightW * InputValue.X + UpW * InputValue.Y).GetSafeNormal();
	if (WorldDir.IsNearlyZero())
	{
		return;
	}

	// 2) 转到 Actor 局部空间（手部偏移是局部坐标，随角色转动）
	const FQuat ActorQuatInv = OwnerCharacter->GetActorQuat().Inverse();
	const FVector LocalDir = ActorQuatInv.RotateVector(WorldDir);

	// 3) 以 MaxMoveStep 作为轴上的最大偶移：Delta 各分量 = LocalDir 各分量 * MaxMoveStep 对应分量
	//    再多一道安全 clamp，允许调用方传入超过 1 的输入也不会超过范围
	FVector Delta(
		LocalDir.X * MaxMoveStep.X,
		LocalDir.Y * MaxMoveStep.Y,
		LocalDir.Z * MaxMoveStep.Z);

	Delta.X = FMath::Clamp(Delta.X, -MaxMoveStep.X, MaxMoveStep.X);
	Delta.Y = FMath::Clamp(Delta.Y, -MaxMoveStep.Y, MaxMoveStep.Y);
	Delta.Z = FMath::Clamp(Delta.Z, -MaxMoveStep.Z, MaxMoveStep.Z);

	// 4) 交替：轮到的那只手从 "基准偏移" 加上 Delta，另一只保持不动
	if (bMoveLeftHandNext)
	{
		LeftHandRuntimeOffset = LeftHandOffset + Delta;
	}
	else
	{
		RightHandRuntimeOffset = RightHandOffset + Delta;
	}

	bMoveLeftHandNext = !bMoveLeftHandNext;
}

bool UClimbComponent::SweepLeftFoot(FHitResult& OutHit) const
{
	return SweepLimbSphere(LeftFootOffset, OutHit, FColor::Purple);
}

bool UClimbComponent::SweepRightFoot(FHitResult& OutHit) const
{
	return SweepLimbSphere(RightFootOffset, OutHit, FColor::Turquoise);
}

void UClimbComponent::TickAutoActiveClimb()
{
	// 节流：按 AutoActiveCheckInterval 间隔执行，避免每帧都做一次扫描
	// <=0 时表示禁用节流，保持每帧检测
	if (AutoActiveCheckInterval > 0.f)
	{
		AutoActiveCheckTimer += LastDeltaTime;
		if (AutoActiveCheckTimer < AutoActiveCheckInterval)
		{
			return;
		}
		AutoActiveCheckTimer = 0.f;
	}

	if (!OwnerCharacter)
	{
		return;
	}

	// 用四个肢体球体分别检测前方可攀爬面
	FHitResult LHHit, RHHit, LFHit, RFHit;
	const bool bLH = SweepLeftHand(LHHit);
	const bool bRH = SweepRightHand(RHHit);
	const bool bLF = SweepLeftFoot(LFHit);
	const bool bRF = SweepRightFoot(RFHit);

	// 在所有命中里挑出 "能映射到合法攀爬类型" 的那个。
	// 优先级：LeftHand -> RightHand -> LeftFoot -> RightFoot。
	const FHitResult* Hits[4]  = { &LHHit, &RHHit, &LFHit, &RFHit };
	const bool        Valid[4] = {  bLH,    bRH,    bLF,    bRF   };

	EClimbType SelectedClimbType = EClimbType::CT_None;
	const FHitResult* SelectedHit = nullptr;
	for (int32 i = 0; i < 4; ++i)
	{
		if (!Valid[i])
		{
			continue;
		}
		const EClimbType CandidateType = GetClimbTypeBySlope(Hits[i]->ImpactNormal);
		if (CandidateType != EClimbType::CT_None)
		{
			SelectedClimbType = CandidateType;
			SelectedHit = Hits[i];
			break;
		}
	}

	if (SelectedClimbType != EClimbType::CT_None && SelectedHit)
	{
		CurrentSurfaceNormal = SelectedHit->ImpactNormal;
		ActiveClimb();
	}
	else
	{
		CancelClimb();
	}
	SwitchClimbType(SelectedClimbType);
}

// Sets default values for this component's properties
UClimbComponent::UClimbComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	OwnerCharacter = Cast<ACharacter>(GetOwner());

	// 手部 Runtime 偏移初始化为基准偏移
	LeftHandRuntimeOffset  = LeftHandOffset;
	RightHandRuntimeOffset = RightHandOffset;
}


// Called when the game starts
void UClimbComponent::BeginPlay()
{
	Super::BeginPlay();

	// 绑定攀爬移动输入：从 Owner（Pawn/Character）的 InputComponent 中取 EnhancedInputComponent 进行绑定
	// 注意：仅做按键绑定，不在此处添加 IMC，避免在未进入攀爬时屏蔽默认移动
	if (OwnerCharacter)
	{
		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(OwnerCharacter->InputComponent))
		{
			if (ClimbMoveAction)
			{
				EIC->BindAction(ClimbMoveAction, ETriggerEvent::Triggered, this, &UClimbComponent::OnClimbMoveInput);
			}
			if (ClimbCancelAction)
			{
				// 使用 Started：只在按下的那一帧触发，避免长按重复调用 CancelClimb
                EIC->BindAction(ClimbCancelAction, ETriggerEvent::Started, this, &UClimbComponent::OnClimbCancelInput);
			}
		}
	}
	// ...
	
}

void UClimbComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 兜底：组件结束时确保攀爬 IMC 被移除，避免泄露到下一个关卡或 PIE
	RemoveClimbMappingContext();

	Super::EndPlay(EndPlayReason);
}


// Called every frame
void UClimbComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	LastDeltaTime = DeltaTime;
	TickAutoActiveClimb();
	// ...
}

EClimbType UClimbComponent::GetClimbTypeBySlope(FVector ImpactNormal) const
{
	//根据法线向量计算坡度角，判断属于哪个攀爬类型
	FVector WorldUp = FVector(0, 0, 1);
	// 2. 计算点积
	float DotVal = FVector::DotProduct(ImpactNormal, WorldUp);
	// 3. 反余弦求弧度，转角度，范围0~180
	float Radian = FMath::Acos(DotVal);
	float SurfaceAngle = FMath::RadiansToDegrees(Radian);
	return ClimbData->GetClimbTypeByAngle(SurfaceAngle);
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

bool UClimbComponent::CanMoveTo(const FVector2D& InputValue, FVector& OutTargetLocation) const
{
	// 默认输出当前位置，便于调用方在返回 false 时仍能安全使用 OutTargetLocation
	const AActor* Owner = GetOwner();
	OutTargetLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;

	// 未激活攀爬或没有有效类型时禁止移动并取消攀爬
	if (!bClimbActive /*|| CurrentClimbType == EClimbType::CT_None*/)
	{
		
		return false;
	}

	// 输入过小视为无效
	if (InputValue.IsNearlyZero())
	{
		return false;
	}

	if (!Owner || !OwnerCharacter)
	{
		return false;
	}

	// 需要 ClimbInfo 才能拿到 MoveSpeed
	const FClimbInfo* Info = GetCurrentClimbInfo();
	if (!Info)
	{
		return false;
	}

	// 以当前墙面法线 N 构造切平面基：
	//   Up_w    = WorldUp 在切平面上的投影（退化时用 ActorForward 投影，避免 0 向量）
	//   Right_w = Up_w × N
	// 2D 输入：X = 沿墙左右，Y = 沿墙上下
	const FVector N = CurrentSurfaceNormal.GetSafeNormal();

	FVector UpW = FVector::VectorPlaneProject(FVector::UpVector, N);
	if (UpW.IsNearlyZero())
	{
		// 地面/天花板等 N 与 WorldUp 平行的退化情况，改用 ActorForward 投影作为"上"
		UpW = FVector::VectorPlaneProject(OwnerCharacter->GetActorForwardVector(), N);
	}
	UpW = UpW.GetSafeNormal();

	const FVector RightW = FVector::CrossProduct(UpW, N).GetSafeNormal();

	const FVector InputDir = (RightW * InputValue.X + UpW * InputValue.Y).GetSafeNormal();
	if (InputDir.IsNearlyZero())
	{
		// 投影后输入方向退化，视为不可移动
		return false;
	}

	// 以当前攀爬类型的 MoveSpeed * DeltaTime 作为本帧位移的模长
	const float Speed = Info->MoveSpeed;
	const float StepLen = Speed * LastDeltaTime;
	if (StepLen <= 0.f)
	{
		return false;
	}

	// 预测目标位置（后续若要做"前方仍是合法攀爬面"等更细致的检测，可在此基础上扩展）
	OutTargetLocation = Owner->GetActorLocation() + InputDir * StepLen;

#if ENABLE_DRAW_DEBUG
	// 调试：用蓝色把"预测位置"画出来，便于直观看出本帧 Move 会落到哪
	if (bDrawDebug)
	{
		if (UWorld* World = GetWorld())
		{
			const bool bPersistent = false;
			const float LifeTime = DebugDrawDuration;
			const FQuat OwnerQuat = Owner->GetActorQuat();

			// 预测位置的球（蓝色），半径用肢体扫描的 SphereRadius 保持一致
			(void)OwnerQuat; // 球体不需要旋转
			DrawDebugSphere(World, OutTargetLocation, SphereRadius, 16,
			                 FColor::Blue, bPersistent, LifeTime, 0, 1.f);

			// 从当前位置指向预测位置的箭头（蓝色）
			DrawDebugDirectionalArrow(World, Owner->GetActorLocation(), OutTargetLocation,
			                          8.f, FColor::Blue, bPersistent, LifeTime, 0, 1.5f);
		}
	}
#endif

	return true;
}

void UClimbComponent::Move(const FVector2D& InputValue)
{
	// 让 CanMoveTo 同时完成"是否可达 + 目标位置预测"
	//移动手部
	MoveHand(InputValue);

	FVector TargetLocation;
	if (!CanMoveTo(InputValue, TargetLocation))
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

	// 直接把 Owner 移动到预测出的目标位置（bSweep=true 让碰撞系统处理穿插）
	Owner->SetActorLocation(TargetLocation, /*bSweep=*/true);

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
	//关闭重力等物理效果的代码可以放在这里
	//预存CMC的移动模式
	CacheMovementMode = OwnerCharacter->GetCharacterMovement()->MovementMode;
	//关闭CMC的移动模式
	OwnerCharacter->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_None);

	// 攀爬激活时才挂载攀爬 IMC，避免平时屏蔽默认移动按键
	AddClimbMappingContext();
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
	CurrentSurfaceNormal = FVector::UpVector;

	// 重置两只手的 Runtime 偏移，避免下次进入攀爬时手仍偷偏移中
	ResetHandRuntimeOffsets();

	//恢复CMC的移动模式
	OwnerCharacter->GetCharacterMovement()->SetMovementMode(CacheMovementMode);

	// 退出攀爬时移除攀爬 IMC，把按键控制权交还给默认移动
	RemoveClimbMappingContext();
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
	//CurrentClimbState = EClimbState::CS_Idle;

	// 进入新类型
	if (NewClimbType != EClimbType::CT_None)
	{
		OnEnterClimbType(NewClimbType);
	}
	UE_LOG(LogTemp, Warning, TEXT("SwitchClimbType: %d"), NewClimbType);
}

void UClimbComponent::OnEnterClimbType_Implementation(EClimbType ClimbType)
{
	// 基类默认不做处理，留给蓝图或子类扩展（例如：切换动画、播放特效等）
}

void UClimbComponent::OnLeaveClimbType_Implementation(EClimbType ClimbType)
{
	// 基类默认不做处理，留给蓝图或子类扩展
}

void UClimbComponent::OnClimbMoveInput(const FInputActionValue& Value)
{
	// 只有攀爬激活时才响应
	if (!bClimbActive || !OwnerCharacter)
	{
		return;
	}

	const FVector2D AxisValue = Value.Get<FVector2D>();
	if (AxisValue.IsNearlyZero())
	{
		return;
	}

	// 2D 输入直接交给 Move，内部会根据当前攀爬类型/朝向进行映射
	Move(AxisValue);
}

void UClimbComponent::OnClimbCancelInput(const FInputActionValue& Value)
{
	// 只有在攀爬激活时才需要手动取消
	if (!bClimbActive)
	{
		return;
	}

	// 打上“用户手动取消”标志，避免 TickAutoActiveClimb 下一帧立刻又把玩家吸回攀爬；
	// 该标志会在玩家离开可攀爬面后由 Tick 自动清除。
	bUserCanceled = true;
	CancelClimb();
}

void UClimbComponent::AddClimbMappingContext()
{
	if (!ClimbMappingContext || !OwnerCharacter)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (!PC)
	{
		return;
	}

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
	{
		Subsystem->AddMappingContext(ClimbMappingContext, MappingPriority);
	}
}

void UClimbComponent::RemoveClimbMappingContext()
{
	if (!ClimbMappingContext || !OwnerCharacter)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (!PC)
	{
		return;
	}

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
	{
		Subsystem->RemoveMappingContext(ClimbMappingContext);
	}
}

