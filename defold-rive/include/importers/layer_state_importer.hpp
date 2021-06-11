#ifndef _RIVE_LAYER_STATE_IMPORTER_HPP_
#define _RIVE_LAYER_STATE_IMPORTER_HPP_

#include "importers/import_stack.hpp"

namespace rive
{
	class LayerState;
	class StateTransition;

	class LayerStateImporter : public ImportStackObject
	{
	private:
		LayerState* m_State;

	public:
		LayerStateImporter(LayerState* state);
		void addTransition(StateTransition* transition);
		StatusCode resolve() override;
	};
} // namespace rive
#endif
