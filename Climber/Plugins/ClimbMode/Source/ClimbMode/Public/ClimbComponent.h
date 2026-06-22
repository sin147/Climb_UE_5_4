// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClimbData.h"
#include "ClimbComponent.generated.h"

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
	float CapsuleRadius;
	float CapsuleHalfHeight;

public:
	// Sets default values for this component's properties
	UClimbComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	EClimbType GetClimbTypeBySlope(FVector NormalVector) const;

	// ---------- 数据 ----------

	/** 攀爬数据资源：存储各 EClimbType -> FClimbInfo 映射 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
	TObjectPtr<UClimbData> ClimbData;

	/** 单次攀爬移动的步长（cm），可在编辑器调整 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb", meta = (ClampMin = "0.0"))
	float MoveStep = 10.f;

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

	// ---------- 核心函数 ----------

	/**
	 * 预测：在当前攀爬类型下，按 InputValue 输入，下一步是否可以移动到。
	 * @param InputValue 玩家输入方向（一般取归一化的方向向量）
	 * @return 可达返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Climb")
	bool CanMoveTo(const FVector& InputValue) const;

	/**
	 * 按 InputValue 执行一次攀爬移动。
	 * 内部会调用 CanMoveTo 进行预测；只有通过预测才会真正移动 Owner。
	 */
	UFUNCTION(BlueprintCallable, Category = "Climb")
	void Move(const FVector& InputValue);

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
};
