/*##############################################################################

##############################################################################*/

//TivaWare uC: Usado internamente para identificar o uC em alguns .h da TivaWare
#define PART_TM4C1294NCPDT 1
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"

#include "driverlib/uart.h"
#include "inc/hw_types.h"

uint32_t ui32SysClkFreq;

//variável que conta os ticks(1ms) - Volatile não permite o compilador otimizar o código 
static volatile unsigned int SysTicks;

//Protótipos de funções criadas no programa, código depois do main
void SetupSystick(void);
void SysTickIntHandler(void);
void setStateRele(int rele, int state);
void initSerial();
void analisaPacote(unsigned char pacote[]);
void printString(unsigned char string[], int len);
void setStateAll(int state);

// Saídas : LED1 | LED2 | LED3 | LED4
//          PN1  | PN0  | PF4  | PF0
// Definições dos reles
#define LIGADO 1
#define DESLIGADO 0
// Definições dos estados dos pacotes
#define PACOTERX_ESPERANDO 0
#define PACOTERX_INICIADO 1
#define PACOTERX_FINALIZADO 2

int main(void)
{
  //Configura o clock para utilizar o xtal interno de 16MHz com PLL para 40MHz
  ui32SysClkFreq = SysCtlClockFreqSet(SYSCTL_OSC_INT | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_320, 40000000);
  //habilitar gpio port N
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
  //aguardar o periférico ficar pronto para uso
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPION)) {/*espera habilitar o port*/}
	 //habilitar gpio port N
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	//aguardar o periférico ficar pronto para uso
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF)) {/*espera habilitar o port*/}
  //LED 1 , LED2, LED 3, LED 4 
  GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1);
  GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_0);
  GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_4);
  GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0);
	
  //executa configuração e inicialização do SysTick
  SetupSystick();
  //loop infinito
	// send chracters to the terminal program
	initSerial();
	
	unsigned char byteReceived;
	unsigned char bufferRx[10];
	int indice = 0;
	unsigned char inicioProtocolo = '#';
	int estado = PACOTERX_ESPERANDO;
  while (1)
        {
					
					if(indice ==  4){
							indice = 0;
					}else{
						byteReceived = UARTCharGet(UART0_BASE);
					}
						
					
					switch(estado){
						case PACOTERX_ESPERANDO:
							if(byteReceived == inicioProtocolo){
								bufferRx[indice] = byteReceived;
								estado = PACOTERX_INICIADO;
								
								indice++;
							}
							break;
						case PACOTERX_INICIADO:
							if(indice == 3){
								bufferRx[indice] = byteReceived;
								estado = PACOTERX_FINALIZADO;
							}else{
								bufferRx[indice] = byteReceived;
							}
							indice++;
							break;
						case PACOTERX_FINALIZADO:
							analisaPacote(bufferRx);
							estado = PACOTERX_ESPERANDO;
							break;
					}
        }
}

//função de tratamento da interrupção do SysTick
void SysTickIntHandler(void)
{
  SysTicks++;
}

//função para configurar e inicializar o periférico Systick a 1ms
void SetupSystick(void)
{
  SysTicks=0;
  //desliga o SysTick para poder configurar
  SysTickDisable();
  //clock 40MHz <=> SysTick deve contar 1ms=40k-1 do Systick_Counter - 12 trocas de contexto PP->IRQ - (1T Mov, 1T Movt, 3T LDR, 1T INC ... STR e IRQ->PP já não contabilizam atrasos para a variável)  
  SysTickPeriodSet(40000-1-12-6);
  //registra a função de atendimento da interrupção
  SysTickIntRegister(SysTickIntHandler);
  //liga o atendimento via interrupção
  SysTickIntEnable();
  //liga novamente o SysTick
  SysTickEnable();
}

/***
*			Inicializa a serial em 115200 
*			
***/
void initSerial(){
	// Enable the UART0 and GPIOA peripherals (the UART pins are on GPIO Port A) 
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	// Configure the pins for the receiver and transmitter
	GPIOPinConfigure(GPIO_PA0_U0RX);
	GPIOPinConfigure(GPIO_PA1_U0TX);
	GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
	UARTConfigSetExpClk(UART0_BASE, ui32SysClkFreq, 115200,(UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
}

/***
*			Liga ou desliga o respectivo rele de 0 a 3, usar os defines LIGADO e DESLIGADO
*			rele = 0...3, state = LIGADO ou desligado
***/
void setStateRele(int rele, int state){
	// Rele de 0 a 3
	// Leds de 1 a 4
	unsigned char respostaAcionamentoRele[5];
	
	switch(rele){
		case 0:
			if(state == LIGADO){
				GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, GPIO_PIN_1);//LED 1
				strcpy((char *)respostaAcionamentoRele, "@R01");
				printString(respostaAcionamentoRele,4);
			}else{
				if(state == DESLIGADO){
				GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);//LED 1
				strcpy((char *)respostaAcionamentoRele, "@R00");
				printString(respostaAcionamentoRele,4);
				}
			}
			break;
		case 1:
			if(state == LIGADO){
				GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);//LED 2
				strcpy((char *)respostaAcionamentoRele, "@R11");
				printString(respostaAcionamentoRele,4);
			}else{
				if(state == DESLIGADO){
					GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0);//LED 2
					strcpy((char *)respostaAcionamentoRele, "@R10");
					printString(respostaAcionamentoRele,4);
				}
			}
      break;
		case 2:
			if(state == LIGADO){
				GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_PIN_4);//LED 3
				strcpy((char *)respostaAcionamentoRele, "@R21");
				printString(respostaAcionamentoRele,4);
			}else{
				if(state == DESLIGADO){
					GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_4, 0);//LED 3
					strcpy((char *)respostaAcionamentoRele, "@R20");
					printString(respostaAcionamentoRele,4);
				}
			}
      break;	
		case 3:
			if(state == LIGADO){
				GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0);//LED 4
				strcpy((char *)respostaAcionamentoRele, "@R31");
				printString(respostaAcionamentoRele,4);
			}else{
				if(state == DESLIGADO){
					GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0);//LED 4
					strcpy((char *)respostaAcionamentoRele, "@R30");
					printString(respostaAcionamentoRele,4);
				}
			}
      break;
	}
}

void setStateAll(int state){
	if(state == LIGADO){
		GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, GPIO_PIN_1);//LED 1
		GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);//LED 2
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_PIN_4);//LED 3
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0);//LED 4
	}else{
		if(state == DESLIGADO){
			GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);//LED 1
			GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0);//LED 2
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_4, 0);//LED 3
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0);//LED 4
		}
	}
}

void statusReles(){
	unsigned char respostaAcionamentoRele[5];
	int R0,R1,R2,R3;
	int B0,B1,B2,B3;
	int i;
	unsigned char statusReles[4];
	char status;
	R0 = GPIOPinRead(GPIO_PORTN_BASE,GPIO_PIN_1);// Retorna 0b0000 0010 0x02 2
	R1 = GPIOPinRead(GPIO_PORTN_BASE,GPIO_PIN_0);// Retorna 0b0000 0001 0x01 1
	R2 = GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_4);// Retorna 0b0001 0000 0x10 16
	R3 = GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_0);// Retorna 0b0000 0001 0x01 1
	
	if(R0 == GPIO_PIN_1)
		B0 = 8;
	else
		B0 = 0;
	
	if(R1 == GPIO_PIN_0)
		B1 = 4;
	else
		B1 = 0;
	
	if(R2 == GPIO_PIN_4)
		B2 = 2;
	else
		B2 = 0;
	
	if(R3 == GPIO_PIN_0)
		B3 = 1;
	else
		B3 = 0;
	
	status = ((B0)|(B1)|(B2)|(B3))+'@';
	//strcpy((char *)respostaAcionamentoRele, statusReles);
	respostaAcionamentoRele[0] = '@';
	respostaAcionamentoRele[1] = 'T';
	respostaAcionamentoRele[2] = 'S';
	respostaAcionamentoRele[3] = status;
	printString(respostaAcionamentoRele,4);
}
	
void analisaPacote(unsigned char pacote[]){
	// b0 = # b1 = R ou T  b2 = 0 a 3 b3 = 1 ou 0
	unsigned char respostaAcionamentoRele[5];
	unsigned char b1,b2,b3;
	int rele, state;
	b1 = pacote[1];
	b2 = pacote[2];
	b3 = pacote[3];
	switch(b1){
		case 'R':
			rele = b2 - '0'; 	//Conversão de ascii para inteiro
			state = b3 - '0';  //Conversão de ascii para inteiro
			if(state == LIGADO){
				setStateRele(rele, LIGADO);
			}else{
				if(state == DESLIGADO){
				 setStateRele(rele, DESLIGADO);
				}
			}
			break;
		case 'T':
			if(b2 == 'X'){
				state = b3 - '0';  //Conversão de ascii para inteiro
				if(state == LIGADO){
					setStateAll(LIGADO);
					strcpy((char *)respostaAcionamentoRele, "@TX1");
					printString(respostaAcionamentoRele,4);
				}else{
					if(state == DESLIGADO){
						setStateAll(DESLIGADO);
						strcpy((char *)respostaAcionamentoRele, "@TX0");
						printString(respostaAcionamentoRele,4);
					}
				}
			}else{
				if(b2 == 'S'&&b3 == 'T'){// Retornar o status de todos os Reles
					statusReles();
				}
			}
			break;
	}
}

void printString(unsigned char string[], int len){
	int i = 0;
	while(i<=(len-1)){
		UARTCharPut(UART0_BASE, string[i]);
		i++;
	}
	UARTCharPut(UART0_BASE, '\n');
}
