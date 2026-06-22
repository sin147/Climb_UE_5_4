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
		if (Pair.Value.SlopeMinAngle <= Angle || Angle <= Pair.Value.SlopeMaxAngle)
		{
			return Pair.Key;
		}
	}
	return EClimbType::CT_None;
}
