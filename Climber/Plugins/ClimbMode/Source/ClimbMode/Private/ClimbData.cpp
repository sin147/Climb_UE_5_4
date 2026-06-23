// Fill out your copyright notice in the Description page of Project Settings.

#include "ClimbData.h"

const FClimbInfo* UClimbData::GetClimbInfo(EClimbType ClimbType) const
{
	return ClimbInfos.Find(ClimbType);
}

const EClimbType UClimbData::GetClimbTypeByAngle(float Angle) const
{
	for(const auto& Pair : ClimbInfos)
	{
		// 区间判断：角度需同时满足 >= Min 且 <= Max，才属于该攀爬类型
		if (Pair.Value.SlopeMinAngle <= Angle && Angle <= Pair.Value.SlopeMaxAngle)
		{
			return Pair.Key;
		}
	}
	return EClimbType::CT_None;
}
