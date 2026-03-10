#include "c4d.h"
#include "c4d_symbols.h"

#include "description/OceanDescription.h"

cinema::Bool RegisterOceanSimulationDescription();
cinema::Bool RegisterOceanSimulationDescription()
{
	return cinema::RegisterDescription(ID_OCEAN_DESCRIPTION, "OceanDescription"_s);
}
