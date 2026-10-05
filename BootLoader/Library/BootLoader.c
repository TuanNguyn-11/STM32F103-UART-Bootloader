#include "Bootloader.h"
#include <stm32f1xx_hal.h>
#include <string.h>
#include <stdint.h>

/* ========== Khai bao UART1 ========== */
extern UART_HandleTypeDef huart1; // Khai bao ben ngoai

/* ========== Dinh nghia Type ========== */
typedef void (*pFunction)(void); // Dinh nghia kieu con tro ham

/* ========== Dinh nghia hang so ========== */
#define ADDR_APP_PROGRAM     0x0800C800   // Dia chi bat dau App
#define PAGE_SIZE            0x400        // 1KB flash page
#define MAX_FW_SIZE          (64*1024)    // 64KB max firmware
#define UART_TIMEOUT_MS      5000         // 5 giay timeout


/* ========== Prototyte ========== */
static void Flash_Erase(uint32_t addr);
static void Flash_Write(uint32_t addr, uint8_t *mData, uint16_t bLen);
static void Erase_Firmware(uint32_t fwLen);
static uint32_t Receive_And_Write_Firmware(uint32_t fwLen);
static uint32_t Calc_Checksum(uint8_t *mData, uint32_t bLen);
static int Is_App_Valid(void);
static void Enter_To_App(void);

/* ========== Ghi-xoa vung nho Flash ========== */
static void Flash_Erase(uint32_t addr) {
    FLASH_EraseInitTypeDef pErase;
    pErase.NbPages = 1;
    pErase.PageAddress = addr;
    pErase.TypeErase = FLASH_TYPEERASE_PAGES;
    uint32_t PError;
    HAL_FLASHEx_Erase(&pErase, &PError);
}

static void Flash_Write(uint32_t addr, uint8_t *mData, uint16_t bLen) {
    uint8_t oFlag = 0; // co bao byte le
    if (bLen % 2 == 1) {
        oFlag = 1;
        bLen--; // Giam ve so chan
    }

    for (uint32_t i = 0; i < bLen; i += 2) {
        // Ghi data theo 1/2 word (16-bit) vidu 1234: 34 -> 12
        uint16_t data = mData[i] | ((uint16_t)mData[i+1] << 8);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, data);
    }

    if (oFlag == 1) {   // Ghi byte le cuoi cung (them 0xFF)
        uint16_t data = mData[bLen] | 0xFF00; 
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + bLen, data);
    }
}

/* ========== Xoa Firmware cu ========== */
static void Erase_Firmware(uint32_t fwLen) {
    HAL_FLASH_Unlock();
    uint32_t erase_addr = ADDR_APP_PROGRAM;
    uint32_t nbPages = (fwLen + PAGE_SIZE - 1) / PAGE_SIZE; // Lam tron len
    
    for (uint32_t page = 0; page < nbPages; page++) {
        Flash_Erase(erase_addr);
        erase_addr += PAGE_SIZE;
    }
    HAL_FLASH_Lock();
}

/* ========== Nhan va ghi Firmware moi ========== */
static uint32_t Receive_And_Write_Firmware(uint32_t fwLen) {
    uint8_t rx_buf[256]; // Buffer dem 256 bytes
    uint32_t addr = ADDR_APP_PROGRAM;
    uint32_t remaining = fwLen;
    uint32_t calc_sum = 0;

    HAL_FLASH_Unlock();
	
    while (remaining > 0) {
        // Tinh do dai Chunk
        uint16_t ChunkLen = (remaining > sizeof(rx_buf)) ? sizeof(rx_buf) : remaining;
            
        if (HAL_UART_Receive(&huart1, rx_buf, ChunkLen, UART_TIMEOUT_MS) != HAL_OK) { 
            // Nhan that bai
            HAL_FLASH_Lock(); // Lock lai truoc khi thoat
            const char *err_msg = "RX_ERR\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t*)err_msg, strlen(err_msg), HAL_MAX_DELAY);
					
						if (Is_App_Valid()) Enter_To_App(); // chay lai app cu
        } 
        else { 
            // Nhan thanh cong -> Ghi Flash
            Flash_Write(addr, rx_buf, ChunkLen);

            calc_sum += Calc_Checksum(rx_buf, ChunkLen);
            addr += ChunkLen;
            remaining -= ChunkLen;
        }
    }
    
    HAL_FLASH_Lock();
    return calc_sum;
}

/* ========== Kiem tra du lieu-chuong trinh =========== */
static uint32_t Calc_Checksum(uint8_t *mData, uint32_t bLen) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < bLen; i++)
        sum += mData[i];
    return sum;
}

static int Is_App_Valid(void) {
    uint32_t sp = *(__IO uint32_t*)ADDR_APP_PROGRAM;            
    uint32_t reset = *(__IO uint32_t*)(ADDR_APP_PROGRAM + 4);      
    
    if (sp == 0xFFFFFFFFU || reset == 0xFFFFFFFFU)
        return 0;    // ko hop le
    return 1;	
}

/* =========== Nhay tu boot sang app =========== */
static void Enter_To_App(void) {
    // Tat cac ngat dang hoat dong
    HAL_RCC_DeInit();
    HAL_DeInit();
    
    // Xoa cac co loi
    SCB->SHCSR &= ~( SCB_SHCSR_USGFAULTENA_Msk | 
                     SCB_SHCSR_BUSFAULTENA_Msk  | 
                     SCB_SHCSR_MEMFAULTENA_Msk );

    // Doi Vector Table ve vung nho App
    SCB->VTOR = ADDR_APP_PROGRAM; 

    // Dat gia tri Main Stack Pointer (MSP)
    __set_MSP(*(__IO uint32_t*) ADDR_APP_PROGRAM); 

    // Lay dia chi Reset Handler cua App va nhay vao
    pFunction app_entry = (pFunction)(*(__IO uint32_t*)(ADDR_APP_PROGRAM + 4));
    app_entry();
}

/* =========== Bootloader qua UART =========== */
void Bootloader_UART(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, 1);   // bat led booloader
    
    // Gui tin hieu san sang
    const char *ready_msg = "BL_READY\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)ready_msg, strlen(ready_msg), HAL_MAX_DELAY);

    // Nhan header (4 bytes Len + 4 bytes Checksum)
    uint8_t header[8]; 
    if (HAL_UART_Receive(&huart1, header, 8, UART_TIMEOUT_MS) != HAL_OK) {
        // Chay App cu
        if (Is_App_Valid()) Enter_To_App();
    }
    else {   // Nhan thanh cong
        uint32_t fwLen = *((uint32_t*)&header[0]); 
        uint32_t fwSum = *((uint32_t*)&header[4]); 

        if (fwLen == 0 || fwLen > MAX_FW_SIZE) { 
            const char *err_msg = "FW_SIZE_ERR\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t*)err_msg, strlen(err_msg), HAL_MAX_DELAY);
					
            if (Is_App_Valid()) Enter_To_App(); // Chay App Cu
        }

        // Xoa App cu
        Erase_Firmware(fwLen);
        
        // Nhan va ghi App moi
        uint32_t calc_sum = Receive_And_Write_Firmware(fwLen);
        
        // Kiem tra Checksum 
        if (calc_sum == fwSum) {
            const char *ok_msg = "FW_OK\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t*)ok_msg, strlen(ok_msg), HAL_MAX_DELAY);
            HAL_Delay(100); // Doi UART gui het
            Enter_To_App();
        } 
        else { 
            const char *fail_msg = "CRC_FAIL\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t*)fail_msg, strlen(fail_msg), HAL_MAX_DELAY);
        }
    }
}