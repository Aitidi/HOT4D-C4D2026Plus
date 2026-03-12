#include "MiniProbe_decl.h"

namespace MiniProbe
{
	class MiniImplementation : public maxon::Component<MiniImplementation, MiniInterface>
	{
		MAXON_COMPONENT();
	};

	// Diagnostic probe intentionally disabled while fixing the main Ocean declaration/class registration path.
}
