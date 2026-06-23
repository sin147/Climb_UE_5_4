// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClimbData.h"
#include "ClimbComponent.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * 攀爬组件
 * 附加到 Actor 上后赋予其攀爬能力：
 *   - 通过 ActiveClimb / CancelClimb 切换攀爬启用状态
 *   - 通过 SwitchClimbType 切换攀爬类型，进入/离开时回调 OnEnter/OnLeaveClimbType
 *   - 每次 Move(InputValue) 都会先用 CanMoveTo(InputValue) 预测下一个位置是否可达
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CLIMBMODE_API UClimbComponent : public UActorComponent
{
	GENERATED_BODY()
private:
	ACharacter* OwnerCharacter = nullptr;	// 缓存的 Owner 的 Character 指针，方便调用相关接口
	EMovementMode CacheMovementMode;
	void TickAutoActiveClimb();

	/** 肢体（手/脚）球体扫描的半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Limb", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float SphereRadius = 15.f;

	/** 球体扫描沿 Forward 推进的距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Limb", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ClimbCheckDistance=80.f;

	/** 四个肢体相对 Actor 的本地偏移（X=前, Y=右, Z=上）。默认按典型人形角色给出近似值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Limb", meta = (AllowPrivateAccess = "true"))
	FVector LeftHandOffset = FVector(0.f, -25.f, 60.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Limb", meta = (AllowPrivateAccess = "true"))
	FVector RightHandOffset = FVector(0.f, 25.f, 60.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Limb", meta = (AllowPrivateAccess = "true"))
	FVector LeftFootOffset = FVector(0.f, -15.f, -80.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Limb", meta = (AllowPrivateAccess = "true"))
	FVector RightFootOffset = FVector(0.f, 15.f, -80.f);

	/** 一次 MoveHand 调用中，手部检测点相对基准位置在三个轴上允许的最大偏移量（按轴 clamp） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Limb", meta = (AllowPrivateAccess = "true"))
	FVector MaxMoveStep = FVector(100.f, 100.f, 100.f);

	/** 运行时实际用于扫描的左/右手本地偏移（会被 MoveHand 交替修改） */
	FVector LeftHandRuntimeOffset;
	FVector RightHandRuntimeOffset;

	/** 下一次 MoveHand 是否该动左手；true=动左手，false=动右手 */
	bool bMoveLeftHandNext = true;

	/**
	 * 通用的肢体球体扫描：
	 * 以 Actor 世界变换将 LocalOffset 转到世界空间作为 Start，沿 Forward * ClimbCheckDistance 推进作为 End，
	 * 用半径 SphereRadius 的球体按 "Pawn" Profile 做一次 SweepSingleByProfile。
	 * 命中返回 true 并写入 OutHit；未命中返回 false。
	 */
	bool SweepLimbSphere(const FVector& LocalOffset, FHitResult& OutHit, FColor DebugColor) const;

	/** TickAutoActiveClimb 的检测间隔（秒），<=0 表示每帧都检测 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AutoActiveCheckInterval = 0.1f;

	/** TickAutoActiveClimb 的累计时间，达到 AutoActiveCheckInterval 才真正执行一次扫描 */
	float AutoActiveCheckTimer = 0.f;

public:
	// Sets default values for this component's properties
	UClimbComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// 在结束时移除 MappingContext
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	EClimbType GetClimbTypeBySlope(FVector NormalVector) const;

	// ---------- 肢体球体扫描接口 ----------

	/** 用球体在左手位置做一次前向扫描，命中返回 true 并写入 OutHit */
	UFUNCTION(BlueprintCallable, Category = "Climb|Limb")
	bool SweepLeftHand(FHitResult& OutHit) const;

	/** 用球体在右手位置做一次前向扫描，命中返回 true 并写入 OutHit */
	UFUNCTION(BlueprintCallable, Category = "Climb|Limb")
	bool SweepRightHand(FHitResult& OutHit) const;

	/** 用球体在左脚位置做一次前向扫描，命中返回 true 并写入 OutHit */
	UFUNCTION(BlueprintCallable, Category = "Climb|Limb")
	bool SweepLeftFoot(FHitResult& OutHit) const;

	/** 用球体在右脚位置做一次前向扫描，命中返回 true 并写入 OutHit */
	UFUNCTION(BlueprintCallable, Category = "Climb|Limb")
	bool SweepRightFoot(FHitResult& OutHit) const;

	// ---------- 数据 ----------

	/** 攀爬数据资源：存储各 EClimbType -> FClimbInfo 映射 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
	TObjectPtr<UClimbData> ClimbData;

	/** 缓存最近一帧的 DeltaTime，用于在 Move 中以 Speed * DeltaTime 推进位移 */
	UPROPERTY(BlueprintReadOnly, Category = "Climb|Runtime")
	float LastDeltaTime = 0.f;

	/** 攀爬时使用的移动输入 Action（Vector2D），在编辑器中赋值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Input")
	TObjectPtr<UInputAction> ClimbMoveAction;

	/** 攀爬时的“手动取消”输入 Action（一般绑定为空格 / Bool），按下时调用 CancelClimb */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Input")
	TObjectPtr<UInputAction> ClimbCancelAction;

	/** 攀爬专用的 Input Mapping Context，会在 BeginPlay 时添加到玩家的 EnhancedInput 子系统 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Input")
	TObjectPtr<UInputMappingContext> ClimbMappingContext;

	/** ClimbMappingContext 的优先级（数值越大优先级越高） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Input")
	int32 MappingPriority = 1;

	/** 是否绘制攀爬检测的调试图形 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Debug")
	bool bDrawDebug = false;

	/** 调试图形的持续时间（秒），<=0 表示只画一帧 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb|Debug", meta = (ClampMin = "0.0"))
	float DebugDrawDuration = 0.f;

	// ---------- 运行时状态（只读） ----------

	/** 攀爬是否处于激活状态 */
	UPROPERTY(BlueprintReadOnly, Category = "Climb|Runtime")
	bool bClimbActive = false;

	/** 当前攀爬类型 */
	UPROPERTY(BlueprintReadOnly, Category = "Climb|Runtime")
	EClimbType CurrentClimbType = EClimbType::CT_None;

	/** 当前攀爬子状态（Idle / Move） */
	UPROPERTY(BlueprintReadOnly, Category = "Climb|Runtime")
	EClimbState CurrentClimbState = EClimbState::CS_Idle;

	/** 当前贴附墙面的法线（由 TickAutoActiveClimb 更新），用于把 2D 输入映射到墙面切平面 */
	UPROPERTY(BlueprintReadOnly, Category = "Climb|Runtime")
	FVector CurrentSurfaceNormal = FVector::UpVector;

	/**
	 * 是否由用户手动取消了攀爬。
	 * 为 true 时 TickAutoActiveClimb 不会自动重新进入攀爬，
	 * 直到本帧检测不到可攀爬面（视为玩家已离开），该标志才会被自动清除。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Climb|Runtime")
	bool bUserCanceled = false;

	// ---------- 核心函数 ----------

	/**
	 * 预测：在当前攀爬类型下，按 InputValue 输入，下一步是否可以移动到。
	 * @param InputValue        玩家输入方向（2D 输入：X=左右，Y=上下/前后）
	 * @param OutTargetLocation 预测到的目标世界坐标（仅当返回 true 时有效；返回 false 时填为当前位置）
	 * @return 可达返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Climb")
	bool CanMoveTo(const FVector2D& InputValue, FVector& OutTargetLocation) const;

	/**
	 * 按 InputValue 执行一次攀爬移动。
	 * 内部会调用 CanMoveTo 进行预测；只有通过预测才会真正把 Owner 移动到预测出的 TargetLocation。
	 */
	UFUNCTION(BlueprintCallable, Category = "Climb")
	void Move(const FVector2D& InputValue);

	/**
	 * 交替移动左/右手的检测位置。
	 * 根据 InputValue 沿当前墙面切平面得到 3D 偶移方向，再乘以 MaxMoveStep 并按轴 clamp 后，
	 * 赋给 "轮到的那只手" 的 Runtime 偏移（以基准偏移为原点）；调用后取反交替标志。
	 */
	UFUNCTION(BlueprintCallable, Category = "Climb|Limb")
	void MoveHand(const FVector2D& InputValue);

	/** 重置两只手的 Runtime 偏移到基准偏移，并重置交替状态 */
	void ResetHandRuntimeOffsets();

	/** 激活攀爬模式 */
	UFUNCTION(BlueprintCallable, Category = "Climb")
	void ActiveClimb();

	/** 取消攀爬模式（同时会触发 OnLeaveClimbType） */
	UFUNCTION(BlueprintCallable, Category = "Climb")
	void CancelClimb();

	/**
	 * 切换攀爬类型。
	 * 若与当前类型不同：先回调 OnLeaveClimbType(旧)，再回调 OnEnterClimbType(新)。
	 */
	UFUNCTION(BlueprintCallable, Category = "Climb")
	void SwitchClimbType(EClimbType NewClimbType);

	/** 进入某个攀爬类型时的回调（可在蓝图覆写扩展） */
	UFUNCTION(BlueprintNativeEvent, Category = "Climb")
	void OnEnterClimbType(EClimbType ClimbType);
	virtual void OnEnterClimbType_Implementation(EClimbType ClimbType);

	/** 离开某个攀爬类型时的回调（可在蓝图覆写扩展） */
	UFUNCTION(BlueprintNativeEvent, Category = "Climb")
	void OnLeaveClimbType(EClimbType ClimbType);
	virtual void OnLeaveClimbType_Implementation(EClimbType ClimbType);

protected:
	/** 内部：根据当前类型获取 ClimbInfo，找不到返回 nullptr */
	const FClimbInfo* GetCurrentClimbInfo() const;

	/** 绑定到 ClimbMoveAction 的回调：把 2D 输入转换成 3D 方向，调用 Move */
	void OnClimbMoveInput(const FInputActionValue& Value);

	/** 绑定到 ClimbCancelAction 的回调：按下时手动取消攀爬 */
	void OnClimbCancelInput(const FInputActionValue& Value);

	/** 将攀爬 IMC 加入到 EnhancedInput 子系统（仅在攀爬激活时调用） */
	void AddClimbMappingContext();

	/** 从 EnhancedInput 子系统中移除攀爬 IMC */
	void RemoveClimbMappingContext();
};
