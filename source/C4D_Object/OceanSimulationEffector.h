
#pragma once

#include "c4d.h"
#include "c4d_baseeffectorplugin.h"
#include "OceanSimulation/OceanSimulation_decl.h"

using namespace cinema;

class OceanSimulationEffector : public EffectorData
{
	INSTANCEOF(OceanSimulationEffector, EffectorData);
public:
	virtual Bool InitEffector(GeListNode* node, Bool isCloneInit) override;

	virtual maxon::Result<maxon::GenericData> InitPoints(const BaseObject* op, const BaseObject* gen, const BaseDocument* doc, const EffectorDataStruct& data, MoData* md, BaseThread* thread) const override;

	virtual void CalcPointValue(const BaseObject* op, const BaseObject* gen, const BaseDocument* doc, const EffectorDataStruct& data, const maxon::GenericData& extraData, MutableEffectorDataStruct& mdata, Int32 index, MoData* md, const Vector& globalpos, Float fall_weight) const override;
	virtual Vector CalcPointColor(const BaseObject* op, const BaseObject* gen, const BaseDocument* doc, const EffectorDataStruct& data, const maxon::GenericData& extraData, const MutableEffectorDataStruct& mdata, Int32 index, MoData* md, const Vector& globalpos, Float fall_weight) const override;

	virtual EXECUTIONRESULT Execute(BaseObject* op, BaseDocument* doc, BaseThread* bt, Int32 priority, EXECUTIONFLAGS flags) override;
	virtual Bool GetDEnabling(const GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) const override;

	static NodeData* Alloc() { return NewObjClear(OceanSimulationEffector); }

private:
	mutable OceanSimulation::OceanRef oceanSimulationRef_; ///< the reference to the ocean simualtion
	mutable maxon::Float currentTime_ = 0.0; ///< store the current time of the animation used in check dirty



	//----------------------------------------------------------------------------------------
	/// Evaluate the result of the simulation and return the vectors for displacemenet, normals and jacobian
	/// @param[in]  op	: the effector send for speed.
	/// @param[in]  p  :  vector of the point to evaluate (only the x and z will be taken into account
	/// @param[out]  displacement  : reference to store the displacement result 
	
	
	/// @return		maxon::OK on success
	//----------------------------------------------------------------------------------------

	maxon::Result<void> EvaluatePoint(const BaseObject* op, const maxon::Vector p, maxon::Vector &displacement) const;

};




