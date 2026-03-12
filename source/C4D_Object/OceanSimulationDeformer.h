#pragma once
//  wavemesh.h
//  
//  Created by Manuel MAGALHAES on 14/01/13.
//  Copyright (c) 2013 Valkaari. All rights reserved.
//

#include "c4d.h"
#include "c4d_falloffdata.h"
#include "OceanSimulation/OceanSimulation_decl.h"

using namespace cinema;

class OceanSimulationDeformer : public ObjectData
{
	INSTANCEOF(OceanSimulationDeformer, ObjectData);
	
private:
	// This is where all the wave action takes place
	mutable maxon::Float				currentTime_ = 0.0; ///< store the current time of the animation used in check dirty
	mutable OceanSimulation::OceanRef	oceanSimulationRef_; ///< ocean reference

	// manage falloff
	mutable C4D_Falloff*			falloff_ = nullptr; ///< the falloff object to be compatible with fields.
	maxon::Int32				falloffDirtyCheck_ = 0; ///< store the checkdirty to see if the fields have changed.
	
	maxon::Float				MapRange(maxon::Float value, const maxon::Float min_input, const maxon::Float max_input, const maxon::Float min_output, const maxon::Float max_output) const;
	Bool						EnsureFalloff(BaseObject* op) const;

public:
	virtual ~OceanSimulationDeformer()
	{
		C4D_Falloff::Free(falloff_);
	}

	virtual Int32				GetHandleCount(const BaseObject *op) const override;
	virtual void				GetHandle(BaseObject *op, Int32 i, HandleInfo &info) override;
	virtual void				SetHandle(BaseObject *op, Int32 i, Vector p, const HandleInfo &info) override;
	virtual Bool				CopyTo(NodeData *dest, const GeListNode *snode, GeListNode *dnode, COPYFLAGS flags, AliasTrans *trn) const override;
	virtual DRAWRESULT			Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh) override;
	
	virtual Bool				GetDDescription(const GeListNode *node, Description *description, DESCFLAGS_DESC &flags) const override;

	virtual void				CheckDirty(BaseObject* op, const BaseDocument* doc) override;
	virtual Bool				GetDEnabling(const GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc) const override;
	virtual Bool				AddToExecution(BaseObject* op, PriorityList* list) override;
	virtual EXECUTIONRESULT		Execute(BaseObject* op, BaseDocument* doc, BaseThread* bt, Int32 priority, EXECUTIONFLAGS flags) override;

	virtual Bool                Init(GeListNode *node, Bool isCloneInit) override;
	// removed free because of the use of reference of ocean object
	// virtual void				Free(GeListNode *node);
	virtual Bool                Message(GeListNode *node, Int32 type, void *t_data) override;
	virtual Bool                ModifyObject(const BaseObject *mod, const BaseDocument *doc, BaseObject *op, const Matrix &op_mg, const Matrix &mod_mg, Float lod, Int32 flags, BaseThread *thread) const override;
	virtual maxon::Result<Bool> GetAccessedObjects(const BaseList2D* node, METHOD_ID method, AccessedObjectsCallback& access) const override;

	static NodeData *Alloc() { return NewObjClear(OceanSimulationDeformer); }
};
