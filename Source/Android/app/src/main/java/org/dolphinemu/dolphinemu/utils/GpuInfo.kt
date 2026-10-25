// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

/** Maps the system GPU to the driver source most likely to have a working package for it. */
object GpuInfo {
    const val VENDOR_ID_QUALCOMM = 0x5143

    private val MODEL_NUMBER = Regex("\\d{3,4}")

    /**
     * @param deviceName Vulkan `deviceName` of the system driver, e.g. "Adreno (TM) 740"
     * @param vendorId Vulkan `vendorID` of the system driver
     * @return the recommended [DriverSource], or `null` when the system driver is the best choice
     */
    fun recommend(deviceName: String?, vendorId: Int?): DriverSource? {
        val name = deviceName.orEmpty()
        if (name.contains("Xclipse", ignoreCase = true)) return GpuDriverCatalog.EXYNOSTOOLS

        val isAdreno = name.contains("Adreno", ignoreCase = true)
        if (isAdreno) {
            val model = MODEL_NUMBER.find(name.substringAfter("Adreno"))?.value?.toIntOrNull()
            when (model?.div(100)) {
                6 -> return GpuDriverCatalog.PURPLE_TURNIP
                7 -> return GpuDriverCatalog.BALEMUNI
                8 -> return GpuDriverCatalog.GAMEHUB_8ELITE
            }
        }
        if (isAdreno || vendorId == VENDOR_ID_QUALCOMM) return GpuDriverCatalog.K11MCH1
        return null
    }
}
