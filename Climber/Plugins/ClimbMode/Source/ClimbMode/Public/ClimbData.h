// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimSequenceBase.h"
#include "ClimbData.generated.h"

/**
 * 攀爬类型枚举
 * 用于区分不同的攀爬场景（缓坡/贴墙/垂直墙/外凸墙/浅顶/全顶）
 */
UENUM(BlueprintType)
enum class EClimbType : uint8
{
	CT_None					UMETA(DisplayName = "None"),
	CT_SlopeCrawl			UMETA(DisplayName = "SlopeCrawl"),			// 缓坡爬行
	CT_LeanWallClimb		UMETA(DisplayName = "LeanWallClimb"),		// 贴墙攀爬
	CT_VerticalWallClimb	UMETA(DisplayName = "VerticalWallClimb"),	// 垂直墙攀爬
	CT_OutwardWallClimb		UMETA(DisplayName = "OutwardWallClimb"),	// 外凸墙攀爬
	CT_ShallowCeilingHang	UMETA(DisplayName = "ShallowCeilingHang"),	// 浅顶悬挂
	CT_FullCeilingHang		UMETA(DisplayName = "FullCeilingHang")		// 全顶悬挂
};

/**
 * 攀爬状态枚举
 * 用于区分当前在该攀爬类型下的子状态（Idle/Move）
 */
UENUM(BlueprintType)
enum class EClimbState : uint8
{
	CS_Idle		UMETA(DisplayName = "Idle"),
	CS_Move		UMETA(DisplayName = "Move")
};

/**
 * 攀爬信息结构体
 * 描述单一 EClimbType 的具体参数：表面坡度角度及各状态对应的动画资源
 */
USTRUCT(BlueprintType)
struct CLIMBMODE_API FClimbInfo
{
	GENERATED_BODY()

	// 该攀爬类型对应表面与水平面之间的角度（度），用于检测平面是否符合该攀爬类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climb", meta = (ClampMax = "180", ClampMin = "0"))
	float SlopeMinAngle = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climb", meta = (ClampMax = "180", ClampMin = "0"))
	float SlopeMaxAngle = 180.f;

	// 该攀爬类型下的移动速度（cm/s）。配合 DeltaTime 推进位移，越倾斜/越费力的类型可调小
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climb", meta = (ClampMin = "0.0"))
	float MoveSpeed = 200.f;

	// 该攀爬类型下，每个攀爬状态对应的动画资源
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climb")
	TMap<EClimbState, TSoftObjectPtr<UAnimSequenceBase>> ClimbAnims;
};

/**
 * 攀爬数据资产
 * 记录每个 EClimbType 对应的 FClimbInfo（坡度角与状态-动画映射）
 * 由 UClimbComponent 引用，用于驱动攀爬逻辑与动画
 */
UCLASS(BlueprintType)
class CLIMBMODE_API UClimbData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 每个攀爬类型对应的具体数据
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climb")
	TMap<EClimbType, FClimbInfo> ClimbInfos;

	/** 根据类型获取攀爬信息，若不存在返回 nullptr */
	const FClimbInfo* GetClimbInfo(EClimbType ClimbType) const;
	const EClimbType GetClimbTypeByAngle(float Angle) const;
};
