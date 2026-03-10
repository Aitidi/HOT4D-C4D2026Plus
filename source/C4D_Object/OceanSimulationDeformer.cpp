
/*
 *  waveMesh.cpp
 *  waves
 *
 *  Created by Manuel MAGALHAES on 23/12/10.
 *  Copyright 2010 Valkaari. All rights reserved.
 *
 */




#include "c4d.h"
#include "c4d_symbols.h"
#include "ge_prepass.h"

#include "maxon/lib_math.h"

#include "maxon/parallelfor.h"

#include "OceanSimulation/OceanSimulation_decl.h"

#include "OceanSimulationDeformer.h"
#include "OOceanDeformer.h"
#include "description/OceanDescription.h"



maxon::Float OceanSimulationDeformer::MapRange(maxon::Float value, const maxon::Float min_input, const maxon::Float max_input, const maxon::Float min_output, const maxon::Float max_output) const
{
	Float inrange = max_input - min_input;

	// Guard against degenerate source ranges before normalizing.
	if (CompareFloatTolerant(inrange, 0.0))
		value = 0.0;
	else
		value = (value - min_input) / inrange;    // Map input range to [0.0 ... 1.0]


	if (value > max_output) 
		return max_output; 
	if (value < min_output) 
		return min_output; 
	return  min_output + (max_output - min_output) * value; // Map to output range and return result

}

Bool OceanSimulationDeformer::Message(GeListNode *node, Int32 type, void *t_data)
{
	if (falloff_)
		falloff_->Message(static_cast<BaseList2D*>(node), type, t_data);

	switch (type) 
	{
		case MSG_MENUPREPARE: {
			((BaseObject*)node)->SetDeformMode(true);
			break;
		}
	
		default:
			break;
	}

	return true;
}




Bool OceanSimulationDeformer::Init(GeListNode *node, Bool isCloneInit)
{

	iferr_scope_handler
	{

		DiagnosticOutput("Error: @", err);
		return false;
	};


	// init the object with some variables in its basecontainer.




	BaseObject		*op = (BaseObject*)node;

	if (isCloneInit)
		return true;

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
	op->SetParameter(CreateDescID(PRE_RUN_FOAM), GeData(false), DESCFLAGS_SET::NONE);  // should be false by default
	op->SetParameter(CreateDescID(DO_CATMU_INTER), GeData(false), DESCFLAGS_SET::NONE);
	op->SetParameter(CreateDescID(DO_JACOBIAN), GeData(false), DESCFLAGS_SET::NONE);
	op->SetParameter(CreateDescID(DO_CHOPYNESS), GeData(true), DESCFLAGS_SET::NONE);
	op->SetParameter(CreateDescID(PSEL_THRES), GeData(0.1), DESCFLAGS_SET::NONE);
	op->SetParameter(CreateDescID(JACOB_THRES), GeData(0.5), DESCFLAGS_SET::NONE);
	op->SetParameter(CreateDescID(FOAM_THRES), GeData(0.03), DESCFLAGS_SET::NONE);
	
	op->SetParameter(CreateDescID(ACTIVE_DEFORM), GeData(true), DESCFLAGS_SET::NONE);

	if (!falloff_)
		falloff_ = C4D_Falloff::Alloc(op->GetDataInstance());
	if (!falloff_)
		return false;
	if (!falloff_->SetMode(FIELDS))
		return false;
	if (!falloff_->InitFalloff(nullptr, op))
		return false;


	

	return true;
}





Bool OceanSimulationDeformer::GetDDescription(const GeListNode *node, Description *description, DESCFLAGS_DESC &flags) const
{
	BaseObject *op = (BaseObject*)node;
	if (!op) 
		return false;
	if (!description->LoadDescription(op->GetType())) 
		return false;

	//---------------------------------
	// Add the falloff interface
	if (falloff_)
	{
		if (!falloff_->SetMode(FIELDS)) 
			return false; // The falloff parameters have to have been setup before it can be added to the description, this like makes sure of that
		if (!falloff_->AddFalloffToDescription(op, description, DESCFLAGS_DESC::NONE)) 
			return false;
	}

	flags |= DESCFLAGS_DESC::LOADED;

	return true;
}


Bool OceanSimulationDeformer::CopyTo(NodeData *dest, const GeListNode *snode, GeListNode *dnode, COPYFLAGS flags, AliasTrans *trn) const
{
	OceanSimulationDeformer *df = (OceanSimulationDeformer*)dest;
	if (!df) 
		return false;
	if (falloff_ && df->falloff_)
		if (!falloff_->CopyTo(df->falloff_, static_cast<BaseList2D*>(dnode))) 
			return false;
	return ObjectData::CopyTo(dest, snode, dnode, flags, trn);
}

DRAWRESULT OceanSimulationDeformer::Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh)
{
	if (!op->GetDeformMode()) 
		return DRAWRESULT::SKIP;
	if (falloff_) 
		falloff_->Draw(op, bd, bh, drawpass);
	return ObjectData::Draw(op, drawpass, bd, bh);
}

Bool OceanSimulationDeformer::GetDEnabling(const GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc) const
{
	if (id[0].id == CURRENTTIME)
	{
		// current Time have to be disable if auto anim is on
		GeData data;
		node->GetParameter(CreateDescID(AUTO_ANIM_TIME), data, DESCFLAGS_GET::NONE);
		return !data.GetBool();
	}

	if (id[0].id == PRE_RUN_FOAM)
	{
		GeData data;
		node->GetParameter(CreateDescID(AUTO_ANIM_TIME), data, DESCFLAGS_GET::NONE);
		return data.GetBool();
	}

	
	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);

}



Int32 OceanSimulationDeformer::GetHandleCount(const BaseObject *op) const
{
	if (falloff_) 
		return falloff_->GetHandleCount();
	return 0;
}

void OceanSimulationDeformer::GetHandle(BaseObject *op, Int32 i, HandleInfo &info)
{
	if (falloff_)  
		falloff_->GetHandle(i, info);
}

void OceanSimulationDeformer::SetHandle(BaseObject *op, Int32 i, Vector p, const HandleInfo &info)
{
	if (falloff_) 
		falloff_->SetHandle(i, p, info);
}

void OceanSimulationDeformer::CheckDirty(BaseObject* op, const BaseDocument* doc)
{

	// fields
	if (falloff_)
	{
		Int32 dirty = falloff_->GetDirty(op, doc);
		if (dirty != falloffDirtyCheck_)
		{
			op->SetDirty(DIRTYFLAGS::DATA);
			falloffDirtyCheck_ = dirty;
		}
	}

	
	// auto time
	maxon::Bool						doAutoTime;
	GeData							data;


	op->GetParameter(CreateDescID(AUTO_ANIM_TIME), data, DESCFLAGS_GET::NONE);
	doAutoTime = data.GetBool();


	if (doAutoTime)
	{
		BaseTime		btCurrentTime = doc->GetTime();
		maxon::Float    currentFrame = btCurrentTime.GetFrame(doc->GetFps());
		if (currentTime_ != currentFrame)
		{
			currentTime_ = currentFrame;
			op->SetDirty(DIRTYFLAGS::DATA);
		}
	}



}


Bool OceanSimulationDeformer::ModifyObject(const BaseObject *mod, const BaseDocument *doc, BaseObject *op, const Matrix &op_mg, const Matrix &mod_mg, Float lod, Int32 flags, BaseThread *thread) const
{

	iferr_scope_handler
	{
		return false;
	};

	if (!op->IsInstanceOf(Opoint) || !falloff_) 
		return true;

	

	maxon::Int32                    pcnt;
	GeData							data;
	VertexColorTag					*jacobmaptag = nullptr;
	VertexColorTag					*foammaptag = nullptr;
	
	/*maxon::Float32                  *jacobpoint = nullptr;
	maxon::Float32                  *foampoint = nullptr;
*/

	VertexColorHandle                  jacobpoint = nullptr;
	VertexColorHandle                  foampoint = nullptr;


	maxon::Float32                  *weight = nullptr;
	maxon::Float                    pselThres;
	maxon::Float                    jacobThres, foamThres;
	BaseSelect						*bsp = nullptr;
	SelectionTag					*stag = nullptr;
	maxon::Vector					p;
	maxon::Vector					dispvalue;
	maxon::Vector					*padr = nullptr;

	maxon::Float					oceanSize, windSpeed, windDirection, shrtWaveLenght, waveHeight, chopAmount, dampReflection, windAlign, oceanDepth, timeScale;
	maxon::Int32					oceanResolution, seed, timeLoop; 
	maxon::Bool						doCatmuInter, doJacobian, doChopyness, doNormals, doAutoTime, preRunFoam; 


	padr = ToPoint(op)->GetPointW();
	pcnt = ToPoint(op)->GetPointCount(); 
	if (!pcnt) 
		return true;
	weight = ToPoint(op)->CalcVertexMap(mod);
	finally{
		DeleteMem(weight);
	};
	

	mod->GetParameter(CreateDescID(OCEAN_RESOLUTION), data, DESCFLAGS_GET::NONE);
	oceanResolution = 1 << data.GetInt32();

	mod->GetParameter(CreateDescID(OCEAN_SIZE), data, DESCFLAGS_GET::NONE);
	oceanSize = data.GetFloat();

	mod->GetParameter(CreateDescID(SHRT_WAVELENGHT), data, DESCFLAGS_GET::NONE);
	shrtWaveLenght = data.GetFloat();

	mod->GetParameter(CreateDescID(WAVE_HEIGHT), data, DESCFLAGS_GET::NONE);
	waveHeight = data.GetFloat();

	mod->GetParameter(CreateDescID(WIND_SPEED), data, DESCFLAGS_GET::NONE);
	windSpeed = data.GetFloat();

	mod->GetParameter(CreateDescID(WIND_DIRECTION), data, DESCFLAGS_GET::NONE);
	windDirection = DegToRad(data.GetFloat());

	mod->GetParameter(CreateDescID(WIND_ALIGNMENT), data, DESCFLAGS_GET::NONE);
	windAlign = data.GetFloat();

	mod->GetParameter(CreateDescID(DAMP_REFLECT), data, DESCFLAGS_GET::NONE);
	dampReflection = data.GetFloat();

	mod->GetParameter(CreateDescID(SEED), data, DESCFLAGS_GET::NONE);
	seed = data.GetInt32();

	mod->GetParameter(CreateDescID(OCEAN_DEPTH), data, DESCFLAGS_GET::NONE);
	oceanDepth = data.GetFloat();

	mod->GetParameter(CreateDescID(CHOPAMOUNT), data, DESCFLAGS_GET::NONE);
	chopAmount = data.GetFloat();

	mod->GetParameter(CreateDescID(TIMELOOP), data, DESCFLAGS_GET::NONE);
	timeLoop = data.GetInt32();

	mod->GetParameter(CreateDescID(TIMESCALE), data, DESCFLAGS_GET::NONE);
	timeScale = data.GetFloat();


	mod->GetParameter(CreateDescID(AUTO_ANIM_TIME), data, DESCFLAGS_GET::NONE);
	doAutoTime = data.GetBool();
	
	if (!doAutoTime)
	{
		// currentTime is set in checkDirty
		mod->GetParameter(CreateDescID(CURRENTTIME), data, DESCFLAGS_GET::NONE);
		const_cast<OceanSimulationDeformer*>(this)->currentTime_ = data.GetFloat();
	}


	mod->GetParameter(CreateDescID(DO_CATMU_INTER), data, DESCFLAGS_GET::NONE);
	doCatmuInter = data.GetBool();

	mod->GetParameter(CreateDescID(DO_JACOBIAN), data, DESCFLAGS_GET::NONE);
	doJacobian = data.GetBool();

	mod->GetParameter(CreateDescID(DO_CHOPYNESS), data, DESCFLAGS_GET::NONE);
	doChopyness = data.GetBool();

	mod->GetParameter(CreateDescID(DO_NORMALS), data, DESCFLAGS_GET::NONE);
	doNormals = data.GetBool() && !doChopyness; // why choppyness ???  normals are not used !!


	mod->GetParameter(CreateDescID(PRE_RUN_FOAM), data, DESCFLAGS_GET::NONE);
	preRunFoam = data.GetBool();

	mod->GetParameter(CreateDescID(JACOBMAP), data, DESCFLAGS_GET::NONE);
	//jacobmaptag = (VertexMapTag*)data.GetLink(doc, Tvertexmap);
	jacobmaptag = (VertexColorTag*)data.GetLink(doc, Tvertexcolor);

	mod->GetParameter(CreateDescID(FOAMMAP), data, DESCFLAGS_GET::NONE);
	//foammaptag = (VertexMapTag*)data.GetLink(doc, Tvertexmap);
	foammaptag = (VertexColorTag*)data.GetLink(doc, Tvertexcolor);

	mod->GetParameter(CreateDescID(PSEL_PARTICLES), data, DESCFLAGS_GET::NONE);
	stag = (SelectionTag*)data.GetLink(doc, Tpointselection);

	mod->GetParameter(CreateDescID(PSEL_THRES), data, DESCFLAGS_GET::NONE);
	pselThres = data.GetFloat();

	mod->GetParameter(CreateDescID(JACOB_THRES), data, DESCFLAGS_GET::NONE);
	jacobThres = data.GetFloat();

	mod->GetParameter(CreateDescID(FOAM_THRES), data, DESCFLAGS_GET::NONE);
	foamThres = data.GetFloat();

	
	mod->GetParameter(CreateDescID(ACTIVE_DEFORM), data, DESCFLAGS_GET::NONE);
	maxon::Bool doDeform = data.GetBool();




	if (jacobmaptag) 
	{

		jacobmaptag->SetPerPointMode(true);


		if (jacobmaptag->GetDataCount() == pcnt) 
		{
			jacobpoint = jacobmaptag->GetDataAddressW();
		}
		else 
		{
			jacobmaptag = nullptr;
			jacobpoint = nullptr;
		}
	}

	if (foammaptag) 
	{
		if (foammaptag->GetDataCount() == pcnt) 
		{
			foampoint = foammaptag->GetDataAddressW();
		}
		else 
		{
			foammaptag = nullptr;
			foampoint = nullptr;
		}
	}


	if (stag) 
	{
		bsp = stag->GetWritableBaseSelect();
		if (bsp) 
			bsp->DeselectAll(); 
	}


	if (oceanSimulationRef_ == nullptr)
	{
		oceanSimulationRef_ = OceanSimulation::Ocean().Create() iferr_return;
	}
	
	


	if (oceanSimulationRef_.NeedUpdate(oceanResolution, oceanSize, shrtWaveLenght, waveHeight, windSpeed, windDirection, windAlign, dampReflection, seed))
	{
		oceanSimulationRef_.Init(oceanResolution, oceanSize, shrtWaveLenght, waveHeight, windSpeed, windDirection, windAlign, dampReflection, seed) iferr_return;
	}
	
	
	oceanSimulationRef_.Animate(currentTime_, timeLoop, timeScale, oceanDepth, chopAmount, true, doChopyness, doJacobian, doNormals) iferr_return;
	
	FieldInput inputs(padr, pcnt, op_mg);
	FieldOutput fieldSamples;
	FalloffDataData falloffData;
	if (!falloff_->InitFalloff(doc, mod, falloffData))
		return false;
	Bool outputsOK = falloff_->PreSample(mod, doc, inputs, fieldSamples, falloffData, FIELDSAMPLE_FLAG::VALUE);


	
	OceanSimulation::INTERTYPE interType = OceanSimulation::INTERTYPE::LINEAR;
	if (doCatmuInter)
		interType = OceanSimulation::INTERTYPE::CATMULLROM;
	  
	Float newMin, newMax; 
	newMin = newMax = 0.0;

	maxon::BaseArray<maxon::Float> storeJminus;
	storeJminus.Resize(pcnt) iferr_return;

	
	
	auto updatePoints = [this, &mod, &padr, &interType, &doChopyness, &doJacobian, &outputsOK, &fieldSamples, &falloffData, &weight, &jacobmaptag, &jacobpoint, &bsp, &pselThres, &storeJminus, &doDeform](maxon::Int32 i)
	{

		iferr_scope_handler
		{
			err.DbgStop();
			return;
		};
		maxon::Vector p = padr[i];
		
		maxon::Vector disp, normal, dispValue;
		maxon::Float jMinus;

		oceanSimulationRef_.EvaluatePoint(interType, p, disp, normal, jMinus) iferr_return;

		Float fallOffSampleValue(1.0);
		if (outputsOK)
			falloff_->Sample(mod, p, &fallOffSampleValue, &fieldSamples, falloffData, true, 0.0, i);
		disp *= fallOffSampleValue;


		if (weight)
			disp *= weight[i]; 

		if (doChopyness)
			p += disp;
		else
			p.y += disp.y;

		if (doJacobian)
		{
			maxon::Float jMinusValue = -jMinus;
			

			if (weight) 
				jMinusValue *= weight[i];
			// jminusValue is stored in the array
			// if (jacobmaptag && jacobpoint) 
			//	jacobpoint[i] = maxon::SafeConvert<maxon::Float32>(jminusvalue);
			
			if (bsp) 
				if (jMinusValue > pselThres) 
					bsp->Toggle(i);

			storeJminus[i] = jMinusValue;
		}
		else if (jacobmaptag && jacobpoint) // tag are present but not the option,  reset the value
		{
			//jacobmaptag->Set(jacobpoint, jacobpoint[i] = 0.0;
			jacobmaptag->Set(jacobpoint, nullptr, nullptr, i, maxon::ColorA32(0.0));
			
		}
		if (doDeform)
			padr[i] = p; // finally update the point
	};
	maxon::ParallelFor::Dynamic(0, pcnt, updatePoints);


	if (jacobmaptag && jacobpoint && doJacobian)
	{
		// get the range of jminus value to set the map in the range of 0-1
		for (auto &jvalue : storeJminus)
		{

			if (jvalue > newMax)
				newMax = jvalue;
			if (jvalue < newMin)
				newMin = jvalue;
		}
		
		
		auto updateTag = [&jacobmaptag, &storeJminus, &jacobpoint, &newMax, &newMin, this](maxon::Int32 i)
		{
			//jacobpoint[i] = maxon::SafeConvert<maxon::Float32>(MapRange(storeJminus[i], newMin, newMax, 0.0, 1.0));
			jacobmaptag->Set(jacobpoint, nullptr, nullptr, i, maxon::ColorA32(maxon::SafeConvert<maxon::Float32>(MapRange(storeJminus[i], newMin, newMax, 0.0, 1.0))));
		};
		maxon::ParallelFor::Dynamic(0, jacobmaptag->GetDataCount(), updateTag);

		jacobmaptag->SetDirty(DIRTYFLAGS::NONE);
	}




	if (foammaptag && foampoint && jacobpoint && jacobmaptag && doJacobian) 
	{

		if (preRunFoam && doAutoTime && currentTime_ == 0.0)
		{
			// run simulation for xx frame and get the jminus vertex map from here.
			// only available in autoTime mode and frame 0.0  (or time offset if implemented)
			// magic number 15 should be change to a UI variable
			// calculate 15 frame before
			maxon::BaseArray<maxon::Float32> foamAtFrameZero;
			foamAtFrameZero.Resize(pcnt) iferr_return;

			// clear the vertex map
			auto clearFoamTag = [&foammaptag, &foampoint ](maxon::Int32 i)
			{
				//foampoint[i] = 0.0;
				foammaptag->Set(foampoint, nullptr, nullptr, i, maxon::ColorA32(0.0));
			};

			maxon::ParallelFor::Dynamic(0, pcnt, clearFoamTag);

			maxon::TimeValue t = maxon::TimeValue::GetTime();
		

			for (maxon::Int32 j = -90 ; j <= 0 ; j++)
			{
				// animate the ocean 
				oceanSimulationRef_.Animate(j, timeLoop, timeScale, oceanDepth, chopAmount, true, doChopyness, doJacobian, doNormals) iferr_return;

				auto getJminus = [this, &padr, &interType, &weight, &storeJminus](maxon::Int32 i)
				{
					iferr_scope_handler
					{
						return;
					};
					maxon::Vector p = padr[i];

					maxon::Vector disp, normal;
					maxon::Float jMinus;

					oceanSimulationRef_.EvaluatePoint(interType, p, disp, normal, jMinus) iferr_return;
					maxon::Float jMinusValue = -jMinus;
					if (weight)
						jMinusValue *= weight[i];
				
					storeJminus[i] = jMinusValue;
				};
				maxon::ParallelFor::Dynamic(0, pcnt, getJminus);

				newMin = newMax = 0.0;
				for (auto &jvalue : storeJminus)
				{
					if (jvalue > newMax)
						newMax = jvalue;
					if (jvalue < newMin)
						newMin = jvalue;
				}
				auto updateTag = [&storeJminus, &newMax, &newMin, this](maxon::Int32 i)
				{
					storeJminus[i] = maxon::SafeConvert<maxon::Float32>(MapRange(storeJminus[i], newMin, newMax, 0.0, 1.0));
				};
				maxon::ParallelFor::Dynamic(0, pcnt, updateTag);


				auto updateFoam = [&foamAtFrameZero, &storeJminus, &jacobThres, &foamThres, this](maxon::Int32 i) {
					if (storeJminus[i] > jacobThres)
						foamAtFrameZero[i] += maxon::SafeConvert<maxon::Float32>((MapRange(storeJminus[i], jacobThres, 1.0, 0.0, 1.0) - foamThres));
					else
						foamAtFrameZero[i] -= maxon::SafeConvert<maxon::Float32>(foamThres);

					foamAtFrameZero[i] = maxon::Clamp01(foamAtFrameZero[i]);

				};
				maxon::ParallelFor::Dynamic(0, pcnt, updateFoam);
				
				

			} // end for foam before
	
			ApplicationOutput("time to calculate the sequence @ ", t.Stop());

			auto updateFoamTag = [&foammaptag, &foampoint, &foamAtFrameZero](maxon::Int32 i)
			{
				maxon::ColorA32 color = maxon::ColorA32(foamAtFrameZero[i]);
				foammaptag->Set(foampoint, nullptr, nullptr, i, color);
			};

			maxon::ParallelFor::Dynamic(0, pcnt, updateFoamTag);


		}
		else
		{

			// calculate normal foam 
			BaseTime currentTime;
			Int32     currentFrame;
			currentTime = doc->GetTime();
			currentFrame = currentTime.GetFrame(doc->GetFps());

			auto updateTag = [&jacobmaptag, &foammaptag, &foampoint, &jacobpoint, &jacobThres, &foamThres, this](maxon::Int32 i) {
					
					if (jacobmaptag->Get(jacobpoint, nullptr, nullptr, i).r > jacobThres)
					{
						//foampoint[i] += maxon::SafeConvert<maxon::Float32>((MapRange(jacobpoint[i], jacobThres, 1, 0, 1) - foamThres));
						maxon::ColorA32 color = foammaptag->Get(foampoint, nullptr, nullptr, i);
						color += maxon::ColorA32(maxon::SafeConvert<maxon::Float32>((MapRange(jacobmaptag->Get(jacobpoint, nullptr, nullptr, i).r , jacobThres, 1.0, 0.0, 1.0) - foamThres)));
						foammaptag->Set(foampoint, nullptr, nullptr, i, color);
					}
					else
					{
						maxon::ColorA32 color = foammaptag->Get(foampoint, nullptr, nullptr, i);
						color -= maxon::ColorA32(maxon::SafeConvert<maxon::Float32>(foamThres));

						foammaptag->Set(foampoint, nullptr, nullptr, i, color);
						//foampoint[i] -= maxon::SafeConvert<maxon::Float32>(foamThres);
			
					}
						maxon::ColorA32 color = foammaptag->Get(foampoint, nullptr, nullptr, i);
						

						foammaptag->Set(foampoint, nullptr, nullptr, i, color.Clamp01());
						//foampoint[i] = maxon::Clamp01(foampoint[i]);

			};
			maxon::ParallelFor::Dynamic(0, pcnt, updateTag);
		}
	}

	
	if (stag) 
	{
		stag->SetDirty(DIRTYFLAGS::DATA);
		stag->Message(MSG_UPDATE);
	}


	op->Message(MSG_UPDATE);
	

	


	return true;
	
	

}



// be sure to use a unique ID obtained from www.plugincafe.com
#define ID_OCEAN_SIMULATION_DEFORMER 1051458
Bool RegisterOceanSimulationDeformer();
Bool RegisterOceanSimulationDeformer()
{

	return RegisterObjectPlugin(ID_OCEAN_SIMULATION_DEFORMER, "HOT 4D"_s, OBJECT_MODIFIER, OceanSimulationDeformer::Alloc, "OOceanDeformer"_s, AutoBitmap("hot4D.tif"_s), 0);
	
}

// helpers

