/***************************************************************************
 *   Copyright (C) 2026 by Alain Carlucci                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include "board_settings.h"
#include "interfaces/arch_registers.h"
#include "stm32c5_psi.h"

using namespace miosix;

struct RCCPSIConfig
{
    OscillatorType oscType;
    unsigned int hse, sysclk;
    unsigned int psiFreq, psiRef, psiRefSrc;
};

   // Supported clock frequencies: 144000000, 100000000

static constexpr RCCPSIConfig psiConfigs[] = {
    {
        //24 MHz HSE -> PSI 144 MHz
        .oscType=OscillatorType::HSE, .hse=24000000, .sysclk=144000000,
        .psiFreq=0b01,          //01: PSI nominal frequency = 144 MHz
        .psiRef=0b011,          //011: 24 MHz (HSE)
        .psiRefSrc=0b00,        //00: HSE clock used as reference for PSI
    },
    {
        //24 MHz HSE -> PSI 100 MHz
        .oscType=OscillatorType::HSE, .hse=24000000, .sysclk=100000000,
        .psiFreq=0b00,          //00: PSI nominal frequency = 100 MHz
        .psiRef=0b011,          //011: 24 MHz (HSE)
        .psiRefSrc=0b00,        //00: HSE clock used as reference for PSI
    },
    {
        //HSI/18 = 8 MHz reference -> PSI 144 MHz  
        .oscType=OscillatorType::HSI, .hse=0, .sysclk=144000000, 
        .psiFreq=0b01,          //00: PSI nominal frequency = 144 MHz
        .psiRef=0b001,          //011: 8 MHz (HSI)
        .psiRefSrc=0b10,        //10: HSI clock divided by 18 used as reference for PSI
    },
    {
        //HSI/18 = 8 MHz reference -> PSI 100 MHz
        .oscType=OscillatorType::HSI, .hse=0, .sysclk=100000000,    
        .psiFreq=0b00,          //00: PSI nominal frequency = 100 MHz
        .psiRef=0b001,          //011: 8 MHz (HSI)
        .psiRefSrc=0b10,        //10: HSI clock divided by 18 used as reference for PSI
    },
};

template<unsigned int N>
constexpr RCCPSIConfig findPsiConfig()
{
    static_assert(N<sizeof(psiConfigs)/sizeof(RCCPSIConfig), "Unsupported sysclk");
    if constexpr(psiConfigs[N].oscType==oscillatorType &&
                 (psiConfigs[N].oscType!=OscillatorType::HSE || psiConfigs[N].hse==hseFrequency) &&
                 psiConfigs[N].sysclk==cpuFrequency) return psiConfigs[N];
    else return findPsiConfig<N+1>();
}

void startPsi()
{

    RCCPSIConfig cfg=findPsiConfig<0>();

    
    // 1. Start the PSI reference 
    if (oscillatorType==OscillatorType::HSE)
    {
        //Enable HSE oscillator
        RCC->CR1 |= RCC_CR1_HSEON;
        while((RCC->CR1 & RCC_CR1_HSERDY)==0) ;    
    } else {
        //Enable HSI oscillator
        RCC->CR1 |= RCC_CR1_HSISON;
        while((RCC->CR1 & RCC_CR1_HSISRDY)==0) ;   
    } 
    // 2. Configure the PSI 
    RCC->CR2 = (cfg.psiFreq << RCC_CR2_PSIFREQ_Pos)
            |  (cfg.psiRef << RCC_CR2_PSIREF_Pos)
            |  (cfg.psiRefSrc << RCC_CR2_PSIREFSRC_Pos);

    // 3. Start the PSI
    RCC->CR1 |= RCC_CR1_PSISON;
    while((RCC->CR1 & RCC_CR1_PSISRDY)==0) ;

    // Increase FLASH wait states 
    constexpr unsigned int latency =
        cpuFrequency<=100000000 ? 2 :   // 68..102 MHz  -> 2 WS
        cpuFrequency<=144000000 ? 4 :   // 136..144 MHz -> 4 WS 
                                  7 ;   // reset value, never used

    constexpr unsigned int wrHighFreq = 
        cpuFrequency<=100000000 ? 0b01 :    // 68..102 MHz
        cpuFrequency<=144000000 ? 0b10 :    // 136..144 MHz
                                  0b10 ;    // never used

    FLASH->ACR = FLASH_ACR_PRFTEN
               | (latency    << FLASH_ACR_LATENCY_Pos)
               | (wrHighFreq << FLASH_ACR_WRHIGHFREQ_Pos);
    while((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != (latency << FLASH_ACR_LATENCY_Pos)) ;

    RCC->CFGR1 |= 0b11 << RCC_CFGR1_SW_Pos;
    while((RCC->CFGR1 & RCC_CFGR1_SWS) != (0b11 << RCC_CFGR1_SWS_Pos)) ;

    SystemCoreClockUpdate();
}
