/*
 *  main.cpp
 *  waves
 *
 *  Created by Manuel MAGALHAES on 23/12/10.
 *  Copyright 2010 Valkaari. All rights reserved.
 *
 */
#include "c4d.h"
#include "main.h"
#include "OceanSimulation/OceanSimulation_decl.h"

namespace OceanSimulation
{
	maxon::Result<OceanRef> CreateOceanDirect();
	maxon::Result<void> RunOceanImplementationSelfTest();
}

namespace cinema
{
Bool PluginStart()
{
	if (!::RegisterOceanSimulationDescription())
		return false;
	if (!::RegisterOceanSimulationDeformer())
		return false;
	if (!::RegisterOceanSimulationEffector())
		return false;

	const auto& oceanDecl = OceanSimulation::Ocean();
	iferr (OceanSimulation::RunOceanImplementationSelfTest())
	{
		ApplicationOutput("HOT4D DEBUG: RunOceanImplementationSelfTest failed: @", err);
	}

	iferr (auto startupOcean = oceanDecl.Create())
	{
		ApplicationOutput("HOT4D DEBUG: Ocean().Create failed, using direct fallback: @", err);
		iferr (auto directOcean = OceanSimulation::CreateOceanDirect())
		{
			ApplicationOutput("HOT4D DEBUG: CreateOceanDirect fallback failed: @", err);
		}
	}

	ApplicationOutput("---------------"_s);
	ApplicationOutput("HOT4D C4D2026Plus adaptation build"_s);
	ApplicationOutput("---------------"_s);

	return true;
}

void PluginEnd()
{
}

Bool PluginMessage(Int32 id, void *data)
{
	switch (id)
	{
		case C4DPL_INIT_SYS:
			if (!g_resource.Init())
				return false;
			return true;

		case C4DMSG_PRIORITY:
			return true;
	}
	
	return false;
}
}
