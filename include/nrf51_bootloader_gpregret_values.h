#ifndef NRF51_BOOTLOADER_GPREGRET_VALUES_H__
#define NRF51_BOOTLOADER_GPREGRET_VALUES_H__

#include <stdbool.h>

#define BOOTLOADER_DFU_GPREGRET_MASK     (0xF8)  /**< Mask for GPGPREGRET bits used for the magic pattern written to GPREGRET register to signal between main app and DFU. */
#define BOOTLOADER_DFU_GPREGRET          (0xB0)  /**< Magic pattern written to GPREGRET register to signal between main app and DFU. The 3 lower bits are assumed to be used for signalling purposes.*/
#define BOOTLOADER_DFU_START_BIT_MASK    (0x01)  /**< Bit mask to signal from main application to enter DFU mode using a buttonless service. */

#define BOOTLOADER_DFU_GPREGRET2_MASK    (0xF8)  /**< Mask for GPGPREGRET2 bits used for the magic pattern written to GPREGRET2 register to signal between main app and DFU. */
#define BOOTLOADER_DFU_GPREGRET2         (0xA8)  /**< Magic pattern written to GPREGRET2 register to signal between main app and DFU. The 3 lower bits are assumed to be used for signalling purposes.*/
#define BOOTLOADER_DFU_SKIP_CRC_BIT_MASK (0x01)  /**< Bit mask to signal from main application that CRC-check is not needed for image verification. */


#define BOOTLOADER_DFU_START    (BOOTLOADER_DFU_GPREGRET | BOOTLOADER_DFU_START_BIT_MASK)      /**< Magic number to signal that bootloader should enter DFU mode because of signal from Buttonless DFU in main app.*/
#define BOOTLOADER_DFU_SKIP_CRC (BOOTLOADER_DFU_GPREGRET2 | BOOTLOADER_DFU_SKIP_CRC_BIT_MASK)  /**< Magic number to signal that CRC can be skipped due to low power modes.*/

#endif // NRF51_BOOTLOADER_GPREGRET_VALUES_H__