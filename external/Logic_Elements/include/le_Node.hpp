#pragma once

#include "le_Base.hpp"

/**
 * @brief Template class for le_Node, representing a node with history.
 *        Inherits from le_Base with a specified type.
 * @tparam T The type of the values stored in the node.
 */
template <typename T>
class le_Node : protected le_Base<T>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the node with a specified history length.
     * @param historyLength The length of the history buffer.
     */
    le_Node(le_Element_Type type, uint16_t historyLength);

    /**
     * @brief Destructor to clean up the node.
     */
    ~le_Node();

    /**
     * @brief Updates the node with the current input value.
     * @param timeStamp The current timestamp.
     */
    virtual void Update(const le_Time& timeStamp) override;

public:
    /**
     * @brief Gets the history buffer with an offset from the circular buffer.
     * @param buffer Pointer to store the history buffer.
     * @param startOffset Pointer to store the start offset in the circular buffer.
     */
    void GetHistory(T* buffer, uint16_t* startOffset);

    /**
     * @brief Connects an output slot of another element to the input of this node.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void SetInput(le_Base<T>* e, uint8_t outputSlot);

    /**
     * @brief Overrides the current value of the node with a specified value for a given duration.
     *
     * This function sets an override value that will replace the current value of the node for a specified duration.
     * The override is initiated immediately and will persist until the duration expires.
     *
     * @tparam T The type of the value to be overridden.
     * @param value The value to override the current node value with.
     * @param duration The duration (in seconds) for which the override value should be used.
     */
    void OverrideValue(T value, float duration);

    /**
     * @brief Checks if override is currently active.
     *
     * @return bool if node is currently overridden.
     */
    bool IsOverridden()
    {
        return this->bOverride;
    }

    using le_Base<T>::GetValue;
    using le_Base<T>::SetValue;

private:
    uint16_t uHistoryLength; ///< The length of the history buffer.
    T* _history;             ///< The history buffer.
    uint16_t uWrite;         ///< The current write position in the history buffer.
    
    float fOverrideTimer;
    float fOverrideDuration;
    T _overrideValue;
    T _overrideOriginalValue;
    bool bOverride;
    le_Time lastTimeStamp;   ///< The last timestamp when Update was called.

    // Allow le_Engine to access private members
    friend class le_Engine;
};

// Helper instantiations for digital and analog nodes

/**
 * @brief Digital node class inheriting from le_Node with bool type.
 */
class le_Node_Digital : public le_Node<bool>
{
public:
    /**
     * @brief Constructor for le_Node_Digital.
     * @param historyLength The length of the history buffer.
     */
    le_Node_Digital(uint16_t historyLength) : le_Node<bool>(le_Element_Type::LE_NODE_DIGITAL, historyLength) {}
};

/**
 * @brief Analog node class inheriting from le_Node with float type.
 */
class le_Node_Analog : public le_Node<float>
{
public:
    /**
     * @brief Constructor for le_Node_Analog.
     * @param historyLength The length of the history buffer.
     */
    le_Node_Analog(uint16_t historyLength) : le_Node<float>(le_Element_Type::LE_NODE_ANALOG, historyLength) {}
};

// Implementation of the le_Node class

/**
 * @brief Constructor implementation that initializes the node with a specified history length.
 * @tparam T The type of the values stored in the node.
 * @param historyLength The length of the history buffer.
 */
template<typename T>
le_Node<T>::le_Node(le_Element_Type type, uint16_t historyLength) : le_Base<T>(type, 1, 1)
{
    // Fixed length
    if (historyLength <= 0)
        historyLength = 1;

    // Set extrinsic variables
    this->uHistoryLength = historyLength;

    // Set intrinsic variables
    this->_history = new T[historyLength];
    this->uWrite = 0;

    // Set override variables
    this->bOverride = false;
    this->fOverrideDuration = 0.0f;
    this->fOverrideTimer = 0.0f;
    this->_overrideOriginalValue = (T)0;
    this->_overrideValue = (T)0;

    // Initialize last timestamp
    this->lastTimeStamp = le_Time();
}

/**
 * @brief Destructor implementation to clean up the node.
 * @tparam T The type of the values stored in the node.
 */
template<typename T>
le_Node<T>::~le_Node()
{
    delete[] this->_history;
}

/**
 * @brief Updates the node with the current input value.
 * @param timeStamp The current timestamp.
 */
template<typename T>
void le_Node<T>::Update(const le_Time& timeStamp)
{
    // Calculate timeStep
    float timeStep = static_cast<float>((le_Time&)timeStamp - this->lastTimeStamp) / 1000000.0f; // Convert microseconds to seconds
    this->lastTimeStamp = timeStamp;

    // If override is set
    if (this->bOverride)
    {
        if (this->fOverrideTimer > this->fOverrideDuration)
        {
            this->SetValue(0, this->_overrideOriginalValue);
            this->bOverride = false;
        }
        else
        {
            // Set output value
            this->SetValue(0, this->_overrideValue);
            this->fOverrideTimer += timeStep;
        }
    }

    // Get input value
    le_Base<T>* e = this->template GetInput<le_Base<T>>(0);
    if (e != nullptr && !this->bOverride)
    {
        T inputValue = e->GetValue(this->GetOutputSlot(0));

        // Set next value
        this->SetValue(0, inputValue);
    }

    // Write to circular buffer and shift write head
    this->_history[this->uWrite] = this->GetValue(0);
    this->uWrite = (this->uWrite - 1 + this->uHistoryLength) % this->uHistoryLength;
}

/**
 * @brief Gets the history buffer with an offset from the circular buffer.
 * @param buffer Pointer to store the history buffer.
 * @param startOffset Pointer to store the start offset in the circular buffer.
 */
template<typename T>
void le_Node<T>::GetHistory(T* buffer, uint16_t* startOffset)
{
    // TODO: Implement this
}

/**
 * @brief Connects an output slot of another element to the input of this node.
 * @param e The element to connect from.
 * @param outputSlot The output slot of the element to connect from.
 */
template<typename T>
void le_Node<T>::SetInput(le_Base<T>* e, uint8_t outputSlot)
{
    // Use default connection function
    le_Element::Connect(e, outputSlot, this, 0);
}

/**
 * @brief Overrides the current value of the node with a specified value for a given duration.
 *
 * This function sets an override value that will replace the current value of the node for a specified duration.
 * The override is initiated immediately and will persist until the duration expires.
 *
 * @tparam T The type of the value to be overridden.
 * @param value The value to override the current node value with.
 * @param duration The duration (in seconds) for which the override value should be used.
 */
template<typename T>
void le_Node<T>::OverrideValue(T value, float duration)
{
    this->_overrideValue = value; ///< The value to override the current node value with.
    this->_overrideOriginalValue = this->GetValue(0);
    this->fOverrideDuration = duration; ///< The duration (in seconds) for which the override value should be used.
    this->fOverrideTimer = 0.0f; ///< Timer to track the duration of the override.
    this->bOverride = true; ///< Flag indicating that the override is active.
}