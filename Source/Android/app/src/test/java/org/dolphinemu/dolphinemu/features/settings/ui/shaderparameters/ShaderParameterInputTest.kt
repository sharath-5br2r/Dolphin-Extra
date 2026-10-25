// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.shaderparameters

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ShaderParameterInputTest {
    @Test
    fun acceptsAPointDecimal() {
        assertEquals(1.5f, ShaderParameterInput.parse("1.5"))
    }

    @Test
    fun acceptsACommaDecimal() {
        // Keyboards for ','-decimal locales type a comma even on a decimal keypad.
        assertEquals(1.5f, ShaderParameterInput.parse("1,5"))
    }

    @Test
    fun acceptsNegativesIntegersAndSurroundingBlanks() {
        assertEquals(-0.25f, ShaderParameterInput.parse("-0.25"))
        assertEquals(3f, ShaderParameterInput.parse("3"))
        assertEquals(2.4f, ShaderParameterInput.parse("  2.4 \n"))
    }

    @Test
    fun rejectsBlankAndMalformedText() {
        assertNull(ShaderParameterInput.parse(""))
        assertNull(ShaderParameterInput.parse("   "))
        assertNull(ShaderParameterInput.parse("abc"))
        assertNull(ShaderParameterInput.parse("1.2.3"))
        assertNull(ShaderParameterInput.parse("1,2.3"))
        assertNull(ShaderParameterInput.parse("-"))
    }

    @Test
    fun rejectsNonFiniteValues() {
        // toFloatOrNull accepts these spellings; a shader uniform must not receive them.
        assertNull(ShaderParameterInput.parse("NaN"))
        assertNull(ShaderParameterInput.parse("Infinity"))
        assertNull(ShaderParameterInput.parse("-Infinity"))
        assertNull(ShaderParameterInput.parse("1e40"))
    }
}
