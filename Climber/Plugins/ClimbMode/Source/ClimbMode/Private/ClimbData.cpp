// Fill out your copyright notice in the Description page of Project Settings.

#include "ClimbData.h"

const FClimbInfo* UClimbData::GetClimbInfo(EClimbType ClimbType) const
{
	return ClimbInfos.Find(ClimbType);
}
