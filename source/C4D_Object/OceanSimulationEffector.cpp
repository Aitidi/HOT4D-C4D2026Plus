
#include "c4d.h"
#include "c4d_symbols.h"
#include "c4d_baseeffectorplugin.h"



#include "description/OceanDescription.h"
#include "OceanSimulation/OceanSimulation_decl.h"




#include "OceanSimulationEffector.h"


#define ID_OCEAN_SIMULATION_EFFECTOR 1051489

Bool OceanSimulationEffector::GetDEnabling(const GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) const
{
	if (id[0].id == CURRENTTIME)
	{
		// current Time have to be disable if auto anim is on
		GeData data;
		node->GetParameter(CreateDescID(AUTO_ANIM_TIME), data, DESCFLAGS_GET::NONE);
		return !data.GetBool();
	}

	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);

}

// Int32 	OceanSimulationEffector::GetEffectorFlags()
// {
//
//	// don't work as i want =)
//	return  EFFECTORFLAGS_TIMEDEPENDENT;
// }




EXECUTIONRESULT 	OceanSimulationEffector::Execute(BaseObject *op, BaseDocument *doc, BaseThread *bt, Int32 priority, EXECUTIONFLAGS flags)
{
	ApplicationOutput("HOT4D DEBUG: Effector::Execute op=@ priority=@ flags=@", op ? op->GetName() : "<null>"_s, priority, flags);

	if (priority != EXECUTIONPRIORITY_EXPRESSION)
		return EXECUTIONRESULT::OK;
	

	maxon::Bool doAutoTime;

	GeData							uiData;
	op->GetParameter(CreateDescID(AUTO_ANIM_TIME), uiData, DESCFLAGS_GET::NONE);
	doAutoTime = uiData.GetBool();


	if (doAutoTime)
	{
		BaseTime btCurrentTime;
		maxon::Float    currentFrame;
		btCurrentTime = doc->GetTime();
		currentFrame = (maxon::Float)btCurrentTime.GetFrame(doc->GetFps());
		if (currentTime_ != currentFrame)
		{
			currentTime_ = currentFrame;
			op->SetDirty(DIRTYFLAGS::DATA);
		}
	}

	return EXECUTIONRESULT::OK;


}


Bool OceanSimulationEffector::InitEffector(GeListNode* node, Bool isCloneInit)
{
	ApplicationOutput("HOT4D DEBUG: Effector::InitEffector nodePtr=@ cloneInit=@", reinterpret_cast<const void*>(node), isCloneInit);
	BaseObject		*op = (BaseObject*)node;
	if (!op)
		return false;


	BaseContainer *bc = op->GetDataInstance();
	if (!bc)
		return false;


	if (!isCloneInit)
	{
		op->SetParameter(CreateDescID(OCEAN_RESOLUTION), GeData(7), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(SEED), GeData(12345), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(OCEAN_SIZE), GeData(400.0), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(WIND_SPEED), GeData(20.0), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(WIND_DIRECTION), GeData(120.0), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(SHRT_WAVELENGHT), GeData(0.01), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(WAVE_HEIGHT), GeData(30.0), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(CHOPAMOUNT), GeData(0.5), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(DAMP_REFLECT), GeData(1.0), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(WIND_ALIGNMENT), GeData(1.0), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(OCEAN_DEPTH), GeData(200.0), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(CURRENTTIME), GeData(0.0), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(TIMELOOP), GeData(90), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(TIMESCALE), GeData(0.5), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(AUTO_ANIM_TIME), GeData(true), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(DO_CATMU_INTER), GeData(false), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(DO_JACOBIAN), GeData(false), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(DO_CHOPYNESS), GeData(true), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(PSEL_THRES), GeData(0.1), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(JACOB_THRES), GeData(0.5), DESCFLAGS_SET::NONE);
		op->SetParameter(CreateDescID(FOAM_THRES), GeData(0.03), DESCFLAGS_SET::NONE);

		bc->SetFloat(ID_MG_BASEEFFECTOR_MINSTRENGTH, -1.0);
		bc->SetBool(ID_MG_BASEEFFECTOR_POSITION_ACTIVE, true);
		bc->SetVector(ID_MG_BASEEFFECTOR_POSITION, Vector(50.0));
	}
	
	iferr_scope_handler{
		err.DiagOutput();
		
		return false;
	};

	if (oceanSimulationRef_ == nullptr)
	{
		ApplicationOutput("HOT4D DEBUG: Effector::InitEffector creating oceanSimulationRef"_s);
		oceanSimulationRef_ = OceanSimulation::Ocean().Create() iferr_return;
	}

	ApplicationOutput("HOT4D DEBUG: Effector::InitEffector completed ok"_s);
	return true;
}

maxon::Result<maxon::GenericData> OceanSimulationEffector::InitPoints(const BaseObject* op, const BaseObject* gen, const BaseDocument* doc, const EffectorDataStruct& data, MoData* md, BaseThread* thread) const
{
	ApplicationOutput("HOT4D DEBUG: Effector::InitPoints opType=@ genType=@", op ? op->GetType() : -1, gen ? gen->GetType() : -1);
	iferr_scope;
	
	const BaseContainer* bc = op->GetDataInstance();
	if (!bc)
		return maxon::GenericData();

	if (oceanSimulationRef_ == nullptr)
	{
		oceanSimulationRef_ = OceanSimulation::Ocean().Create() iferr_return;
	}

	maxon::Float					oceanSize, windSpeed, windDirection, shrtWaveLenght, waveHeight, chopAmount, dampReflection, windAlign, oceanDepth, timeScale;
	maxon::Int32					oceanResolution, seed, timeLoop;
	maxon::Bool						doJacobian, doChopyness, doAutoTime;

	GeData							uiData;
	op->GetParameter(CreateDescID(OCEAN_RESOLUTION), uiData, DESCFLAGS_GET::NONE);
	oceanResolution = 1 << uiData.GetInt32();

	op->GetParameter(CreateDescID(OCEAN_SIZE), uiData, DESCFLAGS_GET::NONE);
	oceanSize = uiData.GetFloat();

	op->GetParameter(CreateDescID(SHRT_WAVELENGHT), uiData, DESCFLAGS_GET::NONE);
	shrtWaveLenght = uiData.GetFloat();

	op->GetParameter(CreateDescID(WAVE_HEIGHT), uiData, DESCFLAGS_GET::NONE);
	waveHeight = uiData.GetFloat();

	op->GetParameter(CreateDescID(WIND_SPEED), uiData, DESCFLAGS_GET::NONE);
	windSpeed = uiData.GetFloat();

	op->GetParameter(CreateDescID(WIND_DIRECTION), uiData, DESCFLAGS_GET::NONE);
	windDirection = DegToRad(uiData.GetFloat());

	op->GetParameter(CreateDescID(WIND_ALIGNMENT), uiData, DESCFLAGS_GET::NONE);
	windAlign = uiData.GetFloat();

	op->GetParameter(CreateDescID(DAMP_REFLECT), uiData, DESCFLAGS_GET::NONE);
	dampReflection = uiData.GetFloat();

	op->GetParameter(CreateDescID(SEED), uiData, DESCFLAGS_GET::NONE);
	seed = uiData.GetInt32();

	op->GetParameter(CreateDescID(OCEAN_DEPTH), uiData, DESCFLAGS_GET::NONE);
	oceanDepth = uiData.GetFloat();

	op->GetParameter(CreateDescID(CHOPAMOUNT), uiData, DESCFLAGS_GET::NONE);
	chopAmount = uiData.GetFloat();

	op->GetParameter(CreateDescID(TIMELOOP), uiData, DESCFLAGS_GET::NONE);
	timeLoop = uiData.GetInt32();

	op->GetParameter(CreateDescID(TIMESCALE), uiData, DESCFLAGS_GET::NONE);
	timeScale = uiData.GetFloat();


	op->GetParameter(CreateDescID(AUTO_ANIM_TIME), uiData, DESCFLAGS_GET::NONE);
	doAutoTime = uiData.GetBool();

	

	if (!doAutoTime)
	{
		op->GetParameter(CreateDescID(CURRENTTIME), uiData, DESCFLAGS_GET::NONE);
		currentTime_ = uiData.GetFloat();
	}

	

	op->GetParameter(CreateDescID(DO_CHOPYNESS), uiData, DESCFLAGS_GET::NONE);
	doChopyness = uiData.GetBool();



	


	if (oceanSimulationRef_.NeedUpdate(oceanResolution, oceanSize, shrtWaveLenght, waveHeight, windSpeed, windDirection, windAlign, dampReflection, seed))
	{
		oceanSimulationRef_.Init(oceanResolution, oceanSize, shrtWaveLenght, waveHeight, windSpeed, windDirection, windAlign, dampReflection, seed) iferr_return;
	}

	oceanSimulationRef_.Animate(currentTime_, timeLoop, timeScale, oceanDepth, chopAmount, true, doChopyness, false, false) iferr_return;
	ApplicationOutput("HOT4D DEBUG: Effector::InitPoints animated currentTime=@", currentTime_);
	return maxon::GenericData();
}

maxon::Result<void> OceanSimulationEffector::EvaluatePoint(const BaseObject* op, const maxon::Vector p, maxon::Vector &displacement) const
{
	iferr_scope;

	maxon::Float waveHeight;
	maxon::Bool doCatmuInter;

	GeData							uiData;
	op->GetParameter(CreateDescID(WAVE_HEIGHT), uiData, DESCFLAGS_GET::NONE);
	waveHeight = uiData.GetFloat();



	op->GetParameter(CreateDescID(DO_CATMU_INTER), uiData, DESCFLAGS_GET::NONE);
	doCatmuInter = uiData.GetBool();

	OceanSimulation::INTERTYPE interType = OceanSimulation::INTERTYPE::LINEAR;
	if (doCatmuInter)
		interType = OceanSimulation::INTERTYPE::CATMULLROM;

	maxon::Vector normal;
	maxon::Float jMinus;
	oceanSimulationRef_.EvaluatePoint(interType, p, displacement, normal, jMinus) iferr_return;
	if (!CompareFloatTolerant(waveHeight, 0.0))
		displacement /= waveHeight; // scale down the result by the wavelegnth so the result should be beetween -1 and 1
	// jMinus /= waveHeight;
	return  maxon::OK;
}


void OceanSimulationEffector::CalcPointValue(const BaseObject* op, const BaseObject* gen, const BaseDocument* doc, const EffectorDataStruct& data, const maxon::GenericData& extraData, MutableEffectorDataStruct& mdata, Int32 index, MoData* md, const Vector& globalpos, Float fall_weight) const
{
	if (index == 0)
		ApplicationOutput("HOT4D DEBUG: Effector::CalcPointValue first point globalpos=(@,@,@)", globalpos.x, globalpos.y, globalpos.z);

	iferr_scope_handler
	{
		err.DbgStop();
		return;
	};
	maxon::Vector disp;
	

	
	EvaluatePoint(op, globalpos, disp) iferr_return;

	mdata._strengthValues.pos = disp;
	mdata._strengthValues.rot = disp;
	mdata._strengthValues.scale = disp;

}

Vector OceanSimulationEffector::CalcPointColor(const BaseObject* op, const BaseObject* gen, const BaseDocument* doc, const EffectorDataStruct& data, const maxon::GenericData& extraData, const MutableEffectorDataStruct& mdata, Int32 index, MoData* md, const Vector& globalpos, Float fall_weight) const
{
	iferr_scope_handler
	{
		err.DbgStop();
		return Vector(0);
	};
	maxon::Vector disp;
	
	

	EvaluatePoint(op, globalpos, disp) iferr_return;


	return disp;

}



Bool RegisterOceanSimulationEffector();
Bool RegisterOceanSimulationEffector()

{
	ApplicationOutput("HOT4D DEBUG: RegisterOceanSimulationEffector called"_s);
	const Bool ok = RegisterEffectorPlugin(ID_OCEAN_SIMULATION_EFFECTOR, "Ocean Simulation Effector"_s, OBJECT_CALL_ADDEXECUTION,
									OceanSimulationEffector::Alloc, "OOceanEffector"_s, AutoBitmap("hot4D_eff.tif"_s), 0);
	ApplicationOutput("HOT4D DEBUG: RegisterOceanSimulationEffector result=@", ok);
	return ok;
}



