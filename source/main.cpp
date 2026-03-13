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
#include "OceanSimulation/MiniProbe_decl.h"

namespace OceanSimulation
{
	maxon::Result<OceanRef> CreateOceanDirect();
	maxon::Result<void> RunOceanImplementationSelfTest();
}


namespace cinema
{
static void RunHot4DPostStartupDiagnostics()
{
	const auto& oceanDecl = OceanSimulation::Ocean();
	const maxon::Bool hasOceanClass = maxon::System::FindDefinition(maxon::EntityBase::TYPE::CLASS, maxon::Id("com.valkaari.OceanSimulation.ocean")) != nullptr;
	const maxon::Bool hasOceanComponent = maxon::System::FindDefinition(maxon::EntityBase::TYPE::COMPONENT, maxon::Id("com.valkaari.OceanSimulation.ocean")) != nullptr;

	iferr (OceanSimulation::RunOceanImplementationSelfTest())
	{
		ApplicationOutput("HOT4D DEBUG: Ocean self-test failed: @", err);
		return;
	}

	iferr (auto startupOcean = oceanDecl.Create())
	{
		ApplicationOutput("HOT4D DEBUG: Ocean().Create failed, using direct fallback: @", err);
		iferr (auto directOcean = OceanSimulation::CreateOceanDirect())
		{
			ApplicationOutput("HOT4D DEBUG: CreateOceanDirect fallback failed: @", err);
		}
		else
		{
			ApplicationOutput("HOT4D DEBUG: Ocean fallback create succeeded (class=@, component=@)"_s, hasOceanClass, hasOceanComponent);
		}
	}
	else
	{
		ApplicationOutput("HOT4D DEBUG: Ocean declaration create succeeded (class=@, component=@)"_s, hasOceanClass, hasOceanComponent);
	}
}

Bool PluginStart()
{
	if (!::RegisterOceanSimulationDescription())
		return false;
	if (!::RegisterOceanSimulationDeformer())
		return false;
	if (!::RegisterOceanSimulationEffector())
		return false;

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

		case C4DPL_STARTACTIVITY:
			RunHot4DPostStartupDiagnostics();
			return true;

		case C4DMSG_PRIORITY:
			return true;
	}
	
	return false;
}
}
