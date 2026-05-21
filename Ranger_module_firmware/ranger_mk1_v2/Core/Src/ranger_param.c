
#include <stdbool.h>
#include <stddef.h>
#include "ranger_param.h"

ranger_param_t g_param =
{
	.reset = 0,
	.voltage = 10,	  		// update with function
	.current = 1000,  		// update with function
	.temperature = 200,
	.step_enable = 0, 		// Is it better to map this to HW sleep?
	.step_microstep = 1,
	.step_dir = 0,
	.step_freq = 1,
	.led1 = 1
};


static const ranger_param_entry_t param_table[] =
{
    { PARAM_VOLTAGE,   		  &g_param.voltage,        		PARAM_I32, PARAM_RO },
	{ PARAM_CURRENT,  		  &g_param.current,        		PARAM_I32, PARAM_RO },
	{ PARAM_TEMPERATURE,  	  &g_param.temperature,        	PARAM_U16, PARAM_RW },
    { PARAM_STEP_ENABLE,      &g_param.step_enable,         PARAM_U8,  PARAM_RW },
    { PARAM_STEP_MICRO,       &g_param.step_microstep,      PARAM_U8,  PARAM_RW },
    { PARAM_STEP_DIR,   	  &g_param.step_dir,         	PARAM_U8,  PARAM_RW },
	{ PARAM_STEP_FREQ,		  &g_param.step_freq,         	PARAM_U32, PARAM_RW },
    { PARAM_LED_PA1,		  &g_param.led1,         		PARAM_U8,  PARAM_RW },
	{ PARAM_RESET,		      &g_param.reset,         		PARAM_U8,  PARAM_RW }
};


const ranger_param_entry_t* ranger_param_find(uint8_t id)
{
    for(uint32_t i = 0; i < sizeof(param_table) / sizeof(param_table[0]); i++)
    {
        if(param_table[i].id == id)
        {
            return &param_table[i];
        }
    }
    return NULL;
}


/*
 * Write a value into a parameter using its parameter ID.
 *
 * Example:
 *   ranger_param_write_uint32(PARAM_STEP_ENABLE, 1);
 *
 * This function:
 *   1. Finds the parameter in the parameter table
 *   2. Checks if the parameter exists
 *   3. Checks if the parameter is writable
 *   4. Writes the value into the correct RAM variable
 *   5. Returns true if successful
 */

bool ranger_param_write(uint8_t id, uint32_t value)
{
    /*
     * Search the parameter table for a matching parameter ID.
     *
     * entry becomes a POINTER to the table entry.
     *
     * Example:
     *   if id = PARAM_STEP_ENABLE
     *
     * entry might point to:
     *
     * {
     *     PARAM_STEP_ENABLE,
     *     &g_param.step_enable,
     *     PARAM_U8,
     *     PARAM_RW
     * }
     */
    const ranger_param_entry_t *entry = ranger_param_find(id);

    /*
     * If no parameter was found,
     * ranger_param_find() returns NULL.
     *
     * NULL means:
     *   "invalid pointer"
     *   "nothing found"
     */
    if(entry == NULL)
    {
        return false;
    }

    /*
     * Check if parameter is writable.
     *
     * PARAM_RW = read/write
     * PARAM_RO = read only
     *
     * If parameter is NOT writable,
     * reject the write request.
     */
    if(entry->access != PARAM_RW)
    {
        return false;
    }

    /*
     * Different parameters have different data sizes:
     *
     * uint8_t   = 8-bit
     * uint16_t  = 16-bit
     * uint32_t  = 32-bit
     *
     * We must write the value differently depending on type.
     */
    switch(entry->type)
    {
        /*
         * Parameter is uint8_t
         */
        case PARAM_U8:

            /*
             * entry->ptr is stored as a generic void*
             *
             * We convert it into:
             *
             *     uint8_t*
             *
             * meaning:
             *
             *     "pointer to uint8_t"
             */
            ((uint8_t*)entry->ptr);

            /*
             * Then:
             *
             *     *((uint8_t*)entry->ptr)
             *
             * means:
             *
             *     "go to that RAM address
             *      and access the actual variable"
             *
             * Finally:
             *
             *     = (uint8_t)value
             *
             * writes the new value into RAM.
             */
            *((uint8_t*)entry->ptr) = (uint8_t)value;

            break;

        /*
         * Parameter is uint16_t
         */
        case PARAM_U16:

            /*
             * Same idea as above,
             * but now treating RAM as uint16_t.
             */
            *((uint16_t*)entry->ptr) = (uint16_t)value;

            break;

        /*
         * Parameter is uint32_t
         */
        case PARAM_U32:

            /*
             * Same idea again,
             * now using uint32_t.
             */
            *((uint32_t*)entry->ptr) = value;

            break;

        /*
         * Parameter is int32_t
         */
        case PARAM_I32:
                *((int32_t*)entry->ptr) = (int32_t)value;
                break;

        /*
         * Unknown parameter type
         */
        default:
            return false;
    }

    /*
     * If we reached here,
     * write operation succeeded.
     */
    return true;
}


/*
 * Read a parameter value using its parameter ID.
 *
 * Example:
 *
 *   uint32_t value;
 *
 *   if(ranger_param_read_uint32(PARAM_VOLTAGE, &value))
 *   {
 *       // value now contains the parameter value
 *   }
 *
 * This function:
 *   1. Finds the parameter in the parameter table
 *   2. Checks if the parameter exists
 *   3. Reads the value from RAM
 *   4. Stores the value into the output variable
 *   5. Returns true if successful
 */

bool ranger_param_read(uint8_t id, uint32_t *value)
{
    /*
     * Search parameter table for matching parameter ID.
     *
     * Example:
     *   PARAM_VOLTAGE
     *
     * entry becomes a POINTER to the matching table entry.
     */
    const ranger_param_entry_t *entry = ranger_param_find(id);

    /*
     * If parameter was not found,
     * ranger_param_find() returns NULL.
     *
     * NULL means:
     *   invalid pointer
     *   nothing found
     */
    if(entry == NULL)
    {
        return false;
    }

    /*
     * Different parameters use different data sizes.
     *
     * We must read RAM differently depending on type.
     */
    switch(entry->type)
    {
        /*
         * Parameter is uint8_t
         */
        case PARAM_U8:

            /*
             * entry->ptr is stored as a generic void*
             *
             * Convert it into:
             *
             *     uint8_t*
             *
             * meaning:
             *
             *     "pointer to uint8_t"
             */
            ((uint8_t*)entry->ptr);

            /*
             * Dereference pointer:
             *
             *     *((uint8_t*)entry->ptr)
             *
             * means:
             *
             *     "go to RAM address
             *      and read the actual variable value"
             *
             * Then store the result into:
             *
             *     *value
             *
             * which is the OUTPUT variable provided by caller.
             */
            *value = *((uint8_t*)entry->ptr);

            break;

        /*
         * Parameter is uint16_t
         */
        case PARAM_U16:

            /*
             * Same idea,
             * now reading RAM as uint16_t.
             */
            *value = *((uint16_t*)entry->ptr);

            break;

        /*
         * Parameter is uint32_t
         */
        case PARAM_U32:

            /*
             * Same idea again,
             * now reading RAM as uint32_t.
             */
            *value = *((uint32_t*)entry->ptr);

            break;

       /*
        * Parameter is int32_t
        */
        case PARAM_I32:
            *value = (uint32_t)(*((int32_t*)entry->ptr));
            break;

        /*
         * Unknown parameter type
         */
        default:
            return false;
    }

    /*
     * If we reached here,
     * read operation succeeded.
     */
    return true;
}
