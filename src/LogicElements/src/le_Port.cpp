#include "le_Port.hpp"
#include "le_Element.hpp"

namespace LogicElements {

/**
 * @brief Constructor for Port.
 * @param name The name of the port.
 * @param type The data type of the port.
 * @param direction The direction of the port (input or output).
 * @param owner The element that owns this port.
 */
Port::Port(const std::string& name, PortType type, PortDirection direction, Element* owner)
    : sName(name), portType(type), portDirection(direction), pOwner(owner)
{
}

} // namespace LogicElements

