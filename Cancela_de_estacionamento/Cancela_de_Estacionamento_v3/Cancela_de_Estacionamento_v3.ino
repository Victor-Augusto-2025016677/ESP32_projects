/*
    - Programação de um sistema de controle de cancela para estacionamento.

    - Matéria: ECAE00 - INTRODUÇÃO À ENGENHARIA E AO MÉTODO CIENTÍFICO
    - Professor: JEREMIAS BARBOSA MACHADO & LUIZ LENARTH GABRIEL VERMAAS
    - Alunos: 
        - Victor Augusto - 2025016677
        - Marco Antônio C Vilela - 2025005299
        - Breno Alves - 2025013746
        - Gustavo Fernandes Gonçalves de Lima - 2025002985
        - Arthur Moraes Marques dos Santos - 2025001389

    Funcionalidades básicas solicitadas:
        - O sistema deve controlar a abertura e fechamento de uma cancela de estacionamento.
        - Deve imprimir um ticket genérico (via monitor serial) quando o botão for pressionado por um motorista na entrada.
        - A impressão do ticket é simulada por um período de 3 segundos.
        - Utiliza um LED verde para indicar 'cancela aberta' e um LED vermelho para 'cancela fechada'.
        - Comunica-se com o operador via monitor serial, informando o status do sistema.

    Mecanismo de segurança adicional:
        - Timeout de 30 segundos: Se um veículo demorar demais para passar pelos sensores (seja na entrada ou na saída), a operação é cancelada e a cancela se fecha para evitar que fique aberta indefinidamente.
        - Sistema Anti-Fraude: O sistema detecta se um segundo veículo tenta aproveitar a cancela aberta do primeiro e a fecha imediatamente para impedir a passagem não autorizada.

    Demais funcionalidades adicionadas:
        - Servo motor adicionado
        - Ticket Detalhado com Numeração: O ticket gerado inclui informações extras como data, hora e um número sequencial único para cada veículo.
        - Contador com Reset Automático: A numeração dos veículos é controlada por um contador interno que é zerado automaticamente a cada 24 horas, permitindo um controle diário.
        - Feedback Detalhado de Status: Fornece mensagens específicas no monitor serial para diversas situações, como 'carro aguardando botão', 'botão pressionado sem carro' e 'sistema ocioso'.
        - Indicador Visual de Impressão: Um LED dedicado (ledImpressao) acende durante a simulação da impressão do ticket, fornecendo um feedback visual da operação.

    Novas funções implementadas:
        - **atualizarDataHora()**: Atualiza as variáveis de data e hora usando NTP ou, em caso de falha, a data/hora de compilação.
        - **FecharCancela()**: Fecha a cancela, atualiza LEDs e estado.
        - **aviso()**: Fecha a cancela com aviso sonoro (buzzer) antes do fechamento.
        - **ImprimirTicket()**: Simula a impressão do ticket, mostra informações detalhadas e acende o LED de impressão.
        - **AbrirCancela()**: Abre a cancela, atualiza LEDs e estado.
        - **SegurancaEntrada()**: Garante que a cancela não fique aberta por muito tempo na entrada (timeout).
        - **SegurancaSaida()**: Garante a segurança na saída, evitando passagem consecutiva e timeout.

    Hardware utilizado:
        - ESP32 - 1
        - Botão - 2
        - chave - 1
        - LEDs - 3 (verde, vermelho e amarelo)
        - Buzzer - 1
        - Fios de conexão
        - 1 servo motor SG90
        - resistores
            - 300 ohms - 3
            - 10k ohms - 3

    Links:
        - Wokwi Projeto: https://wokwi.com/projects/433713661752360961
        - GitHub: https://github.com/Victor-Augusto-2025016677/ESP32_projects.git
*/

COLOQUE O BOTÃO DE TICKET RETIRADO NO PINO 11 ANTES DE COMPILAR E PASSAR O PROJETO PARA A ESP32

// Bibliotecas necessárias para funcionamento do sistema
#include <NTPClient.h>      // Cliente NTP para sincronização de tempo
#include <WiFi.h>           // Biblioteca para conexão Wi-Fi
#include <WiFiUdp.h>        // Biblioteca para comunicação UDP, necessária para o NTPClient
#include <time.h>           // Biblioteca para manipulação de tempo
#include <ESP32Servo.h>     // Biblioteca para controle de servo motor
#include <EEPROM.h>
#include <ESP32Servo.h> // Biblioteca para manipulação da memória EEPROM

// ===================== Configurações de Wi-Fi =====================
const char *ssid     = "Wifi2";      // Nome da rede Wi-Fi
const char *password = "01010101";   // Senha da rede Wi-Fi

#define EEPROM_SIZE 4     // Espaço necessário para um int (4 bytes)
#define ADDR_CONTADOR 0   // Endereço onde o contador será salvo

Servo ServoCancela;
const byte servo = 22;


//Variaveis servo
const int posicaoAberta = 500; // Posição do servo motor para a cancela aberta
const int posicaoFechada = 1495; // Posição do servo motor para a cancela fechada



/*
Para que a ESP conecte-se a esta rede Wi-Fi, crie um hotspot com as seguintes configurações:
    - Nome da rede: Wifi2
    - Senha: 01010101
    - Tipo de segurança: WPA2-PSK
    - Banda: 2.4GHz
*/

// ===================== Inicialização de Objetos =====================
Servo ServoCancela; // Objeto Servo para controle da cancela

WiFiUDP ntpUDP; // Objeto UDP para NTP
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600); // Cliente NTP configurado para UTC-3

// ===================== Variáveis Globais =====================
String dataAtual;         // Armazena a data atual
String horaAtual;         // Armazena a hora atual
String ultimaDataReset = ""; // Armazena a data do último reset do contador

// ===================== Definição dos Pinos =====================
const byte botao = 25;           // Botão de entrada
const byte sensorEntrada = 33;   // Sensor de presença na entrada
const byte sensorSaida = 34;     // Sensor de presença na saída
const byte ledVerde = 27;        // LED verde (cancela aberta)
const byte ledVermelho = 26;     // LED vermelho (cancela fechada)
const byte ledImpressao = 14;    // LED amarelo (impressão do ticket)
const byte buzzer = 12;          // Buzzer para avisos sonoros
const byte servo = 22;           // Pino do servo motor
const byte ticketcancela = 11; // Pino para indicar ticket cancelado

// ===================== Variáveis do Servo Motor =====================
const int posicaoAberta = 500;    // Posição do servo motor para a cancela aberta
const int posicaoFechada = 1495;  // Posição do servo motor para a cancela fechada

// ===================== Variáveis de Controle de Tempo =====================
unsigned long tempoEspera = 0;    // Controle de tempo para mensagens de status
unsigned long tempoEspera1 = 0;   // Controle de tempo para mensagens na entrada
unsigned long tempoEspera2 = 0;   // Controle de tempo para mensagens na saída
unsigned long tempoEspera3 = 0;   // Controle de tempo para mensagens de status (carro aguardando botão)
unsigned long tempoEspera4 = 0; // Controle de tempo para mensagens de status (sistema ocioso)
unsigned long tempoEspera5 = 0; 
bool timeout1 = false;            // Controle de timeout na entrada
bool timeout2 = false;            // Controle de timeout na saída
bool exec1 = false;               // Controle de execução do reset diário

// ===================== Constantes de Tempo =====================
const unsigned long TIMEOUT_ENTRADA = 30000;    // Timeout da entrada (30 segundos)
const unsigned long TIMEOUT_SAIDA = 30000;      // Timeout da saída (30 segundos)
const unsigned long DELAY_IMPRESSAO = 1500;     // Duração da impressão do ticket (1,5s, usado 2x)
const unsigned long DELAY_FECHAMENTO = 2000;    // Duração do fechamento da cancela (2s)
const unsigned long DELAY_CANCELA = 500;        // Duração da abertura/fechamento da cancela (0,5s)
const unsigned long INTERVALO_MSG = 2000;       // Intervalo entre mensagens de status (2s)

// ===================== Variáveis do Buzzer =====================
const int SEGUNDOS_AVISO_BUZZER = 5;            // Quantidade de avisos sonoros antes de fechar a cancela
const unsigned long delaybuzzer = 500;          // Duração do beep do buzzer (0,5s)

// ===================== Outras Variáveis =====================
unsigned int NumeroCarros = 0;      // Contador de carros que entraram no estacionamento
bool cancelaAberta = false;         // Estado atual da cancela (aberta/fechada)

// ===================== Funções Principais =====================

/*
    Função: atualizarDataHora
    - Atualiza as variáveis dataAtual e horaAtual usando o NTP.
    - Caso não haja conexão Wi-Fi, utiliza a data/hora de compilação como fallback.
    - Também inicializa a variável de controle do reset diário.
*/
void atualizarDataHora() {
    if (WiFi.status() == WL_CONNECTED) { // Se Wi-Fi conectado, usa NTP
        timeClient.update();
        time_t rawTime = timeClient.getEpochTime();
        struct tm *timeInfo = localtime(&rawTime);

        char dataBuffer[11];
        char horaBuffer[9];
        strftime(dataBuffer, sizeof(dataBuffer), "%d/%m/%Y", timeInfo);
        strftime(horaBuffer, sizeof(horaBuffer), "%H:%M:%S", timeInfo);

        dataAtual = String(dataBuffer);
        horaAtual = String(horaBuffer);

        if (!exec1) {
            ultimaDataReset = dataAtual;
            exec1 = true;
        }
    } else { // Se Wi-Fi não conectado, usa data/hora de compilação
        struct tm timeInfo = {0};
        strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &timeInfo);

        char dataBuffer[11];
        char horaBuffer[9];
        strftime(dataBuffer, sizeof(dataBuffer), "%d/%m/%Y", &timeInfo);
        strftime(horaBuffer, sizeof(horaBuffer), "%H:%M:%S", &timeInfo);

        dataAtual = String(dataBuffer);
        horaAtual = String(horaBuffer);

        if (!exec1) {
            ultimaDataReset = dataAtual;
            exec1 = true;
        }
    }
}

/*
    Função: FecharCancela
    - Fecha a cancela, atualiza LEDs e estado.
    - Garante que o sistema fique em estado seguro.
*/
void FecharCancela() {
    digitalWrite(ledImpressao, LOW);    // Desliga LED de impressão
    digitalWrite(ledVerde, LOW);        // Desliga LED verde
    digitalWrite(ledVermelho, HIGH);    // Liga LED vermelho
    ServoCancela.writeMicroseconds(posicaoFechada); // Fecha a cancela
    Serial.println("Cancela fechada.\n");
    delay(DELAY_CANCELA);               // Aguarda para simular fechamento
    cancelaAberta = false;              // Atualiza estado
}

/*
    Função: aviso
    - Emite avisos sonoros antes de fechar a cancela.
    - Utilizada em situações de timeout ou fraude.
*/
void aviso() {
    Serial.println("A cancela irá fechar em " + String(SEGUNDOS_AVISO_BUZZER) + " segundos\n");
    for (int i = 0; i < SEGUNDOS_AVISO_BUZZER; i++) {
        digitalWrite(buzzer, HIGH);
        delay(delaybuzzer);
        digitalWrite(buzzer, LOW);
        delay(delaybuzzer);
    }
    FecharCancela();
}

/*
    Função: ImprimirTicket
    - Simula a impressão do ticket de entrada.
    - Mostra informações detalhadas (data, hora, número do carro).
    - Acende o LED de impressão durante o processo.
*/
void ImprimirTicket() {
    Serial.println("Imprimindo ticket...\n");
    digitalWrite(ledImpressao, HIGH);
    delay(DELAY_IMPRESSAO);
    atualizarDataHora();
    Serial.println("| ==================== |");
    Serial.println("| Ticket de Entrada");
    Serial.println("| Data: " + dataAtual);
    Serial.println("| Hora: " + horaAtual);
    Serial.println("| Carro N°: " + String(++NumeroCarros));
    Serial.println("| ==================== |\n");
    delay(DELAY_IMPRESSAO);
    digitalWrite(ledImpressao, LOW);
    Serial.println("Ticket impresso.\n");
}

/*
    Função: AbrirCancela
    - Abre a cancela, atualiza LEDs e estado.
    - Utilizada após a impressão do ticket.
*/
void AbrirCancela() {
    Serial.println("Abrindo cancela...\n");
    digitalWrite(ledImpressao, LOW);
    digitalWrite(ledVermelho, LOW);
    digitalWrite(ledVerde, HIGH);
    ServoCancela.writeMicroseconds(posicaoAberta); // Abre a cancela
    Serial.println("Cancela aberta.\n");
    delay(DELAY_CANCELA);
    cancelaAberta = true;
}

/*
    Função: SegurancaEntrada
    - Garante que a cancela não fique aberta por muito tempo na entrada.
    - Fecha a cancela automaticamente após timeout.
    - Exibe mensagens de status enquanto o carro está na entrada.
*/
void SegurancaEntrada() {
    timeout1 = false;
    tempoEspera1 = millis();
    unsigned long tempoInicio = millis();

    while (digitalRead(sensorEntrada)) {
        if (millis() - tempoInicio >= TIMEOUT_ENTRADA) {
            Serial.println("Tempo limite excedido. Cancelando operação.\n");
            aviso();
            timeout1 = true;
            break;
        }
        if (millis() - tempoEspera1 >= INTERVALO_MSG) {
            Serial.println("Carro ainda posicionado na entrada...\n");
            tempoEspera1 = millis();
        }
    }

    if (!digitalRead(sensorEntrada)) {
        Serial.println("Carro saiu da entrada.\n");
    }
}

/*
    Função: SegurancaSaida
    - Garante a segurança na saída, evitando passagem consecutiva e timeout.
    - Fecha a cancela imediatamente em caso de fraude.
    - Exibe mensagens de status enquanto o carro não sai completamente.
*/
void SegurancaSaida() {
    timeout2 = false;
    tempoEspera2 = millis();
    unsigned long tempoInicio1 = millis();

    while (!digitalRead(sensorSaida) && !timeout1) {
        if (millis() - tempoInicio1 >= TIMEOUT_SAIDA) {
            Serial.println("Tempo limite excedido. Cancelando operação.\n");
            aviso();
            timeout2 = true;
            break;
        }
        if (millis() - tempoEspera2 >= INTERVALO_MSG) {
            Serial.println("Carro ainda não saiu completamente...\n");
            tempoEspera2 = millis();
        }
    }

    if (digitalRead(sensorEntrada) && digitalRead(sensorSaida)) {
        Serial.println("Carro detectado na entrada e saída. Fechando cancela imediatamente para impedir passagem consecutiva sem emissão de ticket.\n");
        FecharCancela();
    }
    else if (digitalRead(sensorSaida)) {
        Serial.println("Carro detectado na saída. Fechando cancela dentro de 2 segundos.\n");
        delay(DELAY_FECHAMENTO);
        FecharCancela();
    }
}

void segurancaTicketCancela() 
{
    while (digitalRead(ticketcancela) == HIGH) 
    {
        
        if (digitalRead(ticketcancela) == HIGH && millis() - tempoEspera5 >= INTERVALO_MSG) 44
        {
        Serial.println("Ticket ainda não foi retirado. Por favor, retire o ticket para abrir a cancela.");
        tempoEspera5 = millis();
        }

    }
    Serial.println("Ticket retirado. Cancela pode ser aberta.");
}

/*
    Função: setup
    - Inicializa o sistema, configura pinos, conecta ao Wi-Fi e inicializa o servo motor.
    - Lê o contador de carros da EEPROM.
    - Define o estado inicial da cancela (fechada).
*/
void setup() {
    EEPROM.begin(EEPROM_SIZE); // Inicializa EEPROM
    EEPROM.get(ADDR_CONTADOR, NumeroCarros); // Recupera contador salvo

    Serial.begin(115200); // Inicializa comunicação serial

    // Configuração dos pinos
    pinMode(botao, INPUT);
    pinMode(sensorEntrada, INPUT);
    pinMode(sensorSaida, INPUT);
    pinMode(ticketcancela, INPUT);
    pinMode(ledVerde, OUTPUT);
    pinMode(ledVermelho, OUTPUT);
    pinMode(ledImpressao, OUTPUT);
    pinMode(buzzer, OUTPUT);

    // Conexão Wi-Fi
    WiFi.begin(ssid, password);
    int a = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        a++;
        if (a > 20) {
            break;
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi conectado!\n");
        timeClient.begin();
    }

    // Inicialização do servo motor
    ServoCancela.setPeriodHertz(50);
    ServoCancela.attach(servo, posicaoAberta, posicaoFechada);
    ServoCancela.writeMicroseconds(posicaoAberta); // Cancela aberta
    delay(2000);
    ServoCancela.writeMicroseconds(posicaoFechada); // Cancela fechada
    delay(2000);

    // Estado inicial: cancela fechada, LEDs configurados
    digitalWrite(ledImpressao, LOW);
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledVermelho, HIGH);
    ServoCancela.writeMicroseconds(posicaoFechada);
    cancelaAberta = false;

    Serial.println("Sistema de Controle de Cancela Iniciado.");
}

/*
    Função: loop
    - Loop principal do sistema.
    - Gerencia o fluxo de entrada de veículos, impressão de ticket, abertura/fechamento da cancela e mensagens de status.
    - Realiza o reset diário do contador de carros.
*/
void loop() {   
    atualizarDataHora();
    bool entradaAtiva = digitalRead(sensorEntrada); // Leitura do sensor de entrada
    bool botaoPressionado = digitalRead(botao);     // Leitura do botão

    // Reset diário do contador de carros ao virar o dia
    if (dataAtual != ultimaDataReset) {
        NumeroCarros = 0;
        EEPROM.put(ADDR_CONTADOR, 0);
        EEPROM.commit();
        ultimaDataReset = dataAtual;
        Serial.println("Contador de carros resetado: novo dia detectado.\n");
    }

    // Fluxo principal: carro presente e botão pressionado
    if (entradaAtiva && botaoPressionado) {
        Serial.println("Botão pressionado.\n");
        ImprimirTicket();
        segurancaTicketCancela(); // Aguarda a retirada do ticket
        AbrirCancela();
        SegurancaEntrada();
        SegurancaSaida();

        // Se houve timeout, desfaz o incremento do contador
        if (timeout1 || timeout2) {
            Serial.println("Operação cancelada devido ao timeout. Contador de carros não incrementado.\n");
            if (NumeroCarros > 0) {
                NumeroCarros--;
            }
        } else {
            // Operação bem-sucedida, salva novo valor na EEPROM
            EEPROM.put(ADDR_CONTADOR, NumeroCarros);
            EEPROM.commit();
            Serial.println("Carro registrado com sucesso.\n");
            Serial.println("Sistema pronto para novo veículo.\n");
        }
    }
    // Botão pressionado sem carro presente
    else if (!entradaAtiva && botaoPressionado && millis() - tempoEspera >= INTERVALO_MSG) {
        Serial.println("Botão pressionado, mas não há carro na posição.");
        Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
        tempoEspera = millis();
    }
    // Carro presente, aguardando botão
    else if (entradaAtiva && !botaoPressionado && millis() - tempoEspera3 >= INTERVALO_MSG) {
        Serial.println("há carro na posição, aguardando pressionamento do botão...");
        Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
        tempoEspera3 = millis();
    }
    // Sistema ocioso (idle)
    else if (!entradaAtiva && !botaoPressionado && millis() - tempoEspera4 >= INTERVALO_MSG) {
        Serial.println("Não há carro na posição, sistema em idle (Aguardando Veiculos)");
        Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
        tempoEspera4 = millis();
    }
}
