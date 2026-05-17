# Traballo Tutelado 2 — PWM + LED + Sensor de luz (FRDM-KL46Z)

## Enunciado

Programa unha aplicación para a placa FRDM-KL46Z que faga uso de PWM para
acender os LEDs con maior ou menor intensidade en función do valor do **sensor
de luz** da placa (opción ADC).

- Con **pouca luz**: o LED verde está acendido ao máximo e o vermello apagado.
- Con **moita luz**: o LED vermello está ao máximo e o verde apagado.
- **Polo medio**: luminosidade proporcional e inversa entre os dous LEDs.

O sensor presenta una sensibilidade baixa, e precisa dunha fonte de luz preto do sensor para ver os cambios.

## Compilación e flasheo

```bash
make build    # compilar → main.elf
make flash    # flashear via OpenOCD/CMSIS-DAP
```

## Estrutura

| Arquivo      | Contido                                              |
|--------------|------------------------------------------------------|
| `main.c`     | Init UART/TPM0/ADC0, bucle de lectura e control LEDs |
| `startup.c`  | Vector table e ResetHandler (sen cambios)            |
| `link.ld`    | Script de linkado (sen cambios)                      |
| `makefile`   | Regras de compilación                                |


## Bibliografía

- **NXP/Freescale**, *KL46 Sub-Family Reference Manual* (`KL46P121M48SF4RM.pdf`,
  Rev. 3, 2013) — capítulos TPM (§31), ADC (§28) e SIM/port mux (§12).

- **ARM Limited**, *Application Note AN179: CRC computation using ARM cores*
  (ARM DAI 0179B) — referencia de traballo anterior.
