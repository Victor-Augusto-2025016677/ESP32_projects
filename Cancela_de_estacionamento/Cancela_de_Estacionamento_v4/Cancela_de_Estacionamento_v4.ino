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

    Funcionalidades basicas solicitadas:
        - O sistema deve controlar a abertura e fechamento de uma cancela de estacionamento.
        - Deve imprimir um ticket genérico (via monitor serial) quando o botão for pressionado por um motorista na entrada.
        - A impressão do ticket é simulada por um período de 3 segundos.
        - Utiliza um LED verde para indicar 'cancela aberta' e um LED vermelho para 'cancela fechada'.
        - Comunica-se com o operador via monitor serial, informando o status do sistema.

    Mecanismo de segurança adicional:
        - Timeout de 30 segundos: Se um veículo demorar demais para passar pelos sensores (seja na entrada ou na saída), a operação é cancelada e a cancela se fecha para evitar que fique aberta indefinidamente.
        - Sistema Anti-Fraude: O sistema detecta se um segundo veículo tenta aproveitar a cancela aberta do primeiro e a fecha imediatamente para impedir a passagem não autorizada.

    Demais funcionalidades adicionadas:
        - Ticket Detalhado com Numeração: O ticket gerado inclui informações extras como data, hora e um número sequencial único para cada veículo.
        - Contador com Reset Automático: A numeração dos veículos é controlada por um contador interno que é zerado automaticamente a cada 24 horas, permitindo um controle diário.
        - Feedback Detalhado de Status: Fornece mensagens específicas no monitor serial para diversas situações, como 'carro aguardando botão', 'botão pressionado sem carro' e 'sistema ocioso'.
        - Indicador Visual de Impressão: Um LED dedicado (ledImpressao) acende durante a simulação da impressão do ticket, fornecendo um feedback visual da operação.

    Hardware utilizado:
        - ESP32 - 1
        - Botão - 2
        - chave - 1
        - LEDs - 3 (verde, vermelho e amarelo)
        - Buzzer - 1
        - Fios de conexão
        - Sensor Ultrassônico HC-SR04 - 1
        - resistores
            - 300 ohms - 3
            - 10k ohms - 3

    Links:
        -Wokwi Projeto: https://wokwi.com/projects/433759616809691137
        -GitHub: https://github.com/Victor-Augusto-2025016677/ESP32_projects.git
*/

// ===================== Bibliotecas =====================
//#include <Arduino.h> // Somente necessário para o platformio, caso contrario, não é necessário, pois o Arduino IDE já inclui essa biblioteca por padrão.
#include <NTPClient.h>      // Cliente NTP para sincronização de tempo
#include <WiFi.h>           // Biblioteca para conexão Wi-Fi
#include <WiFiUdp.h>        // Biblioteca para comunicação UDP, necessária para o NTPClient
#include <time.h>           // Biblioteca para manipulação de tempo
#include <Ultrasonic.h>     // Biblioteca para o sensor ultrassônico HC-SR04
#include <EEPROM.h>         // Biblioteca para manipulação da memória EEPROM
#include <ESP32Servo.h> // Biblioteca para controle de servo motor

// ===================== Configurações de Wi-Fi =====================
const char *ssid     = "Wifi2";      // Nome da rede Wi-Fi
const char *password = "01010101";   // Senha da rede Wi-Fi

#define EEPROM_SIZE 4     // Espaço necessário para um int (4 bytes)
#define ADDR_CONTADOR 0   // Endereço onde o contador será salvo

// ===================== Inicialização do cliente NTP =====================
WiFiUDP ntpUDP; // Objeto UDP para o NTP
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600); // Cliente NTP configurado para UTC-3

// ===================== Variáveis Globais =====================
String dataAtual;             // Armazena a data atual
String horaAtual;             // Armazena a hora atual
String ultimaDataReset = "";  // Armazena a data do último reset do contador

// ===================== Definição dos Pinos =====================
const byte botao = 15;           // Botão de entrada
const byte alavancamanual = 21;  // Alavanca para controle manual
const byte sensorSaida = 18;     // Sensor de presença na saída
const byte ledVerde = 27;        // LED verde (cancela aberta)
const byte ledVermelho = 26;     // LED vermelho (cancela fechada)
const byte ledImpressao = 13;    // LED amarelo (impressão do ticket)
const byte buzzer = 12;          // Buzzer para avisos sonoros
const byte ticketcancela = 11;   // Pino para indicar ticket cancelado (não utilizado no código)
const byte trig1 = 16;           // Pino TRIG do sensor ultrassônico
const byte echo1 = 17;           // Pino ECHO do sensor ultrassônico
bool entradaAtiva = false;       // Indica se há carro na entrada
const byte servo = 22;

Servo ServoCancela;

//Variaveis servo
const int posicaoAberta = 500; // Posição do servo motor para a cancela aberta
const int posicaoFechada = 1495; // Posição do servo motor para a cancela fechada


// ===================== Sensor Ultrassônico =====================
Ultrasonic ultrasonic1(trig1, echo1); // Instancia o sensor ultrassônico
const int cmpresente = 5;            // Distância em cm para considerar presença de veículo
const int cmsaida = 15;

// ===================== Variáveis de Controle de Tempo =====================
unsigned long tempoEspera = 0;    // Controle de tempo para mensagens de status
unsigned long tempoEspera1 = 0;   // Controle de tempo para mensagens na entrada
unsigned long tempoEspera2 = 0;   // Controle de tempo para mensagens na saída
unsigned long tempoEspera3 = 0;   // Controle de tempo para mensagens de status (carro aguardando botão)
unsigned long tempoEspera4 = 0;   // Controle de tempo para mensagens de status (sistema ocioso)
unsigned long tempoEspera5 = 0;   // Controle de tempo para mensagens no modo manual
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

// ===================== Funções =====================

/*
    Função: atualizarDataHora
    --------------------------------------
    - Atualiza as variáveis dataAtual e horaAtual usando o NTP.
    - Caso não haja conexão Wi-Fi, utiliza a data/hora de compilação como fallback.
    - Também inicializa a variável de controle do reset diário.
*/
void atualizarDataHora() {
    if (WiFi.status() == WL_CONNECTED) { // Se Wi-Fi conectado, usa NTP
        timeClient.update(); // Atualiza o cliente NTP
        time_t rawTime = timeClient.getEpochTime(); // Obtém o tempo atual em segundos desde 1970
        struct tm *timeInfo = localtime(&rawTime); // Converte para estrutura de data/hora local

        char dataBuffer[11]; // Buffer para armazenar a data formatada
        char horaBuffer[9];  // Buffer para armazenar a hora formatada
        strftime(dataBuffer, sizeof(dataBuffer), "%d/%m/%Y", timeInfo); // Formata a data
        strftime(horaBuffer, sizeof(horaBuffer), "%H:%M:%S", timeInfo); // Formata a hora

        dataAtual = String(dataBuffer); // Atualiza a variável global com a data
        horaAtual = String(horaBuffer); // Atualiza a variável global com a hora

        if (!exec1) { // Se ainda não executou o reset diário
            ultimaDataReset = dataAtual; // Salva a data do reset
            exec1 = true; // Marca que já executou
        }
    } else { // Se Wi-Fi não conectado, usa data/hora de compilação
        struct tm timeInfo = {0}; // Inicializa a estrutura de tempo
        strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &timeInfo); // Converte a data/hora de compilação

        char dataBuffer[11];
        char horaBuffer[9];
        strftime(dataBuffer, sizeof(dataBuffer), "%d/%m/%Y", &timeInfo); // Formata a data
        strftime(horaBuffer, sizeof(horaBuffer), "%H:%M:%S", &timeInfo); // Formata a hora

        dataAtual = String(dataBuffer); // Atualiza a variável global com a data
        horaAtual = String(horaBuffer); // Atualiza a variável global com a hora

        if (!exec1) { // Se ainda não executou o reset diário
            ultimaDataReset = dataAtual; // Salva a data do reset
            exec1 = true; // Marca que já executou
        }
    }
}

/*
    Função: FecharCancela
    --------------------------------------
    - Fecha a cancela do estacionamento.
    - Atualiza os LEDs: verde e impressão desligados, vermelho ligado.
    - Atualiza o estado da variável cancelaAberta.
    - Exibe mensagem no monitor serial.
*/
void FecharCancela() {
    digitalWrite(ledImpressao, LOW); // Desliga o LED de impressão
    digitalWrite(ledVerde, LOW);     // Desliga o LED verde
    digitalWrite(ledVermelho, HIGH);
    ServoCancela.writeMicroseconds(posicaoFechada);
    Serial.println("Cancela fechada.\n"); // Mensagem de confirmação de fechamento da cancela
    delay(DELAY_CANCELA);                 // Mantém a cancela fechada por 0,5 segundo
    cancelaAberta = false;                // Define o estado da cancela como fechada
}

/*
    Função: aviso
    --------------------------------------
    - Emite avisos sonoros antes de fechar a cancela.
    - Utiliza o buzzer para avisar o usuário por alguns segundos.
    - Após os avisos, chama FecharCancela().
    - Exibe mensagens no monitor serial.
*/
void aviso() {
    Serial.println("A cancela irá fechar em " + String(SEGUNDOS_AVISO_BUZZER) + " segundos\n"); // Mensagem de aviso

    for (int i = 0; i < SEGUNDOS_AVISO_BUZZER; i++) // Repete pelo número de avisos configurado
    {
        digitalWrite(buzzer, HIGH); // Liga o buzzer
        delay(delaybuzzer);         // Espera 500ms
        digitalWrite(buzzer, LOW);  // Desliga o buzzer
        delay(delaybuzzer);         // Espera 500ms
    }

    FecharCancela(); // Fecha a cancela após os avisos
}

/*
    Função: ImprimirTicket
    --------------------------------------
    - Simula a impressão do ticket de entrada.
    - Atualiza data/hora.
    - Incrementa o contador de carros.
    - Liga o LED de impressão durante o processo.
    - Exibe informações detalhadas do ticket no monitor serial.
*/
void ImprimirTicket() {
    atualizarDataHora(); // Atualiza data e hora antes de imprimir
    Serial.println("Imprimindo ticket...\n"); 
    digitalWrite(ledImpressao, HIGH); // Liga o LED de impressão
    delay(DELAY_IMPRESSAO);           // Aguarda o tempo de impressão

    Serial.println("| ==================== |"); // Delimitação estética do ticket
    Serial.println("| Ticket de Entrada");
    Serial.println("| Data: " + dataAtual); // Data da emissão do ticket
    Serial.println("| Hora: " + horaAtual); // Hora da emissão do ticket
    Serial.println("| Carro N°: " + String(++NumeroCarros)); // Incrementa e mostra o número do carro
    Serial.println("| ==================== |\n");
    delay(DELAY_IMPRESSAO);           // Aguarda o tempo de impressão (totalizando 3s)
    digitalWrite(ledImpressao, LOW);  // Desliga o LED de impressão
    Serial.println("Ticket impresso.\n"); // Mensagem de confirmação de impressão
}

/*
    Função: AbrirCancela
    --------------------------------------
    - Abre a cancela do estacionamento.
    - Atualiza os LEDs: verde ligado, vermelho e impressão desligados.
    - Atualiza o estado da variável cancelaAberta.
    - Exibe mensagem no monitor serial.
*/
void AbrirCancela() {
    Serial.println("Abrindo cancela...\n"); // Mensagem de abertura
    digitalWrite(ledImpressao, LOW); // Desliga o LED de impressão
    digitalWrite(ledVermelho, LOW);  // Desliga o LED vermelho
    digitalWrite(ledVerde, HIGH);    // Liga o LED verde
    ServoCancela.writeMicroseconds(posicaoAberta);
    Serial.println("Cancela aberta.\n"); // Mensagem de confirmação de abertura da cancela
    delay(DELAY_CANCELA);                // Delay de 0,5 segundo para simular a abertura da cancela
    cancelaAberta = true;                // Define o estado da cancela como aberta
}

/*
    Função: SegurancaEntrada
    --------------------------------------
    - Garante que a cancela não fique aberta por muito tempo na entrada.
    - Monitora o tempo de permanência do veículo na entrada.
    - Se exceder o tempo limite, cancela a operação e fecha a cancela.
    - Exibe mensagens de status no monitor serial.
*/
void SegurancaEntrada() {
    timeout1 = false; // Reseta o timeout para a entrada
    tempoEspera1 = millis(); // Reseta o tempo para evitar repetição de mensagens
    unsigned long tempoInicio = millis(); // Armazena o tempo de início da operação

    while (((ultrasonic1.read() <= cmpresente) == true) && ((ultrasonic1.read() >= cmsaida) == false)) // Enquanto há um carro na entrada
    {
        if (millis() - tempoInicio >= TIMEOUT_ENTRADA) // Se excedeu o tempo limite
        {
            Serial.println("Tempo limite excedido. Cancelando operação.\n"); // Mensagem de cancelamento
            aviso(); // Fecha a cancela, com avisos
            timeout1 = true; // Define o timeout como verdadeiro
            break; // Encerra o while
        }
        if (millis() - tempoEspera1 >= INTERVALO_MSG) // Mensagem a cada INTERVALO_MSG
        {
            Serial.println("Carro ainda posicionado na entrada...\n"); // Mensagem de aviso
            tempoEspera1 = millis(); // Atualiza o tempo de espera
        }
    }

    if ((ultrasonic1.read() >= cmsaida) == true) // Se o carro saiu da entrada
    {
        Serial.println("Carro saiu da entrada.\n");
    }
}

/*
    Função: SegurancaSaida
    --------------------------------------
    - Garante a segurança na saída do veículo.
    - Monitora o tempo de permanência do veículo na saída.
    - Fecha a cancela após a saída ou em caso de tentativa de fraude.
    - Exibe mensagens de status no monitor serial.
*/
void SegurancaSaida() {
    timeout2 = false; // Reseta o timeout para a saída
    tempoEspera2 = millis(); // Reseta o tempo para mensagens
    unsigned long tempoInicio1 = millis(); // Salva o tempo para o timeout

    while (digitalRead(sensorSaida) == false && timeout1 == false) // Enquanto o carro não sair
    {
        if (millis() - tempoInicio1 >= TIMEOUT_SAIDA) // Evento de timeout
        {
            Serial.println("Tempo limite excedido. Cancelando operação.\n"); // Aviso de cancelamento
            aviso(); // Fecha a cancela, com avisos
            timeout2 = true; // Define o timeout como verdadeiro
            break;
        }
        if (millis() - tempoEspera2 >= INTERVALO_MSG) // Mensagem a cada INTERVALO_MSG
        {
            Serial.println("Carro ainda não saiu completamente...\n"); // Mensagem de feedback
            tempoEspera2 = millis();
        }
    }

    if (((ultrasonic1.read() <= cmpresente) == true) && digitalRead(sensorSaida)) // Carro na entrada e saída
    {
        Serial.println("Carro detectado na entrada e saída. Fechando cancela imediatamente para impedir passagem consecutiva sem emissão de ticket.\n");
        FecharCancela();
    }
    else if (digitalRead(sensorSaida)) // Carro na saída
    {
        Serial.println("Carro detectado na saída. Fechando cancela dentro de 2 segundos.\n");
        delay(DELAY_FECHAMENTO);  
        FecharCancela();
    }
}

/*
    Função: manual
    --------------------------------------
    - Permite o controle manual da cancela via alavanca.
    - Abre a cancela enquanto a alavanca estiver ativada.
    - Fecha a cancela ao desativar a alavanca.
    - Exibe mensagens de status no monitor serial.
*/
void manual() {
    bool ativacaomanual = digitalRead(alavancamanual); // Lê a alavanca de ativação manual
    bool execunica = false; // Controle para executar apenas uma vez

    while(digitalRead(alavancamanual) == HIGH) // Enquanto a alavanca estiver ativada
    {
        if (execunica == false)
        {
            Serial.println("Alavanca manual ativada. Cancela será aberta manualmente.\n");
            AbrirCancela(); // Abre a cancela manualmente
            execunica = true; // Evita repetição
        }
        if (millis() - tempoEspera5 >= INTERVALO_MSG) // Mensagem a cada INTERVALO_MSG
        {
            Serial.println("Cancela em modo MANUAL"); // Mensagem de feedback
            Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n"); // Status da cancela
            tempoEspera5 = millis();
        }
    }
    if (execunica == true) // Se a alavanca foi ativada
    {
        Serial.println("Alavanca manual desativada. Cancela será fechada.\n");
        FecharCancela(); // Fecha a cancela manualmente
    }
}

/*
    Função: setup
    --------------------------------------
    - Inicializa o sistema, configura pinos e conecta ao Wi-Fi.
    - Recupera o contador de carros da EEPROM.
    - Inicializa LEDs e exibe mensagem de início no monitor serial.
*/
void setup() {
    EEPROM.begin(EEPROM_SIZE); // Inicializa EEPROM
    EEPROM.get(ADDR_CONTADOR, NumeroCarros); // Recupera contador salvo
    Serial.begin(115200); // Inicia o serial

    // Define os pinos como entrada ou saída
    pinMode(botao, INPUT);
    pinMode(alavancamanual, INPUT);
    pinMode(sensorSaida, INPUT);
    pinMode(ledVerde, OUTPUT);
    pinMode(ledVermelho, OUTPUT);
    pinMode(ledImpressao, OUTPUT);
    pinMode(buzzer, OUTPUT);

    // Conexão com a rede Wi-Fi
    WiFi.begin(ssid, password);
    int a = 0;
    while (WiFi.status() != WL_CONNECTED) { // Aguarda conexão Wi-Fi
        delay(500);
        Serial.print(".");
        a++;
        if (a > 20) { // Timeout para não travar o setup
            break;
        }
    }
    if (WiFi.status() == WL_CONNECTED) { // Se conectou, inicia o cliente NTP
        Serial.println("\nWiFi conectado!\n");
        timeClient.begin();
    }

    //Servo motor
    ServoCancela.setPeriodHertz(50);
    ServoCancela.attach(servo, posicaoAberta, posicaoFechada);
    ServoCancela.writeMicroseconds(posicaoAberta); // Cancela aberta
    delay(2000);
    ServoCancela.writeMicroseconds(posicaoFechada); // Cancela fechada


        // Cancela inicia fechada
    digitalWrite(ledImpressao, LOW);
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledVermelho, HIGH);
    ServoCancela.writeMicroseconds(posicaoFechada);
    cancelaAberta = false;
    
    // Mensagem que o sistema iniciou
    Serial.println("");
    Serial.println("Sistema de Controle de Cancela Iniciado.");
}

/*
    Função: loop
    --------------------------------------
    - Loop principal do sistema.
    - Gerencia o fluxo de entrada e saída de veículos.
    - Controla o reset diário do contador.
    - Exibe mensagens de status no monitor serial.
*/
void loop() {
    atualizarDataHora(); // Atualiza data e hora a cada ciclo
    bool botaoPressionado = digitalRead(botao);     // Leitura do botão de entrada
    bool ativacaomanual = digitalRead(alavancamanual); // Leitura da alavanca manual

    if (ativacaomanual == true) // Se a alavanca manual for ativada, chama a função de controle manual
    {
        manual(); // Chama a função de controle manual
    }

    if (ultrasonic1.read() <= cmpresente) // Se o sensor ultrassônico detectar um carro na posição de entrada
    {
        entradaAtiva = true; // Define a variável entradaAtiva como verdadeira
    }
    else
    {
        entradaAtiva = false; // Caso contrário, define como falsa
    }

    // Reset diário do contador de carros
    if (dataAtual != ultimaDataReset) {
        NumeroCarros = 0;
        EEPROM.put(ADDR_CONTADOR, 0);
        EEPROM.commit();
        ultimaDataReset = dataAtual;
        Serial.println("Contador de carros resetado: novo dia detectado.\n");
    }

    // Fluxo principal de entrada de veículo
    if (entradaAtiva && botaoPressionado) // Se há carro na posição de entrada e o botão foi pressionado
    {
        Serial.println("Botão pressionado.\n"); // Mensagem que o botão foi pressionado
        ImprimirTicket(); // Inicia a impressão do ticket
        AbrirCancela(); // Abre a cancela
        SegurancaEntrada(); // Chama a função de segurança da entrada
        SegurancaSaida(); // Chama a função de segurança da saída

        if (timeout1 || timeout2) { // Se houve timeout, desfaz incremento do contador
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
    // Botão pressionado sem carro na entrada
    else if (!entradaAtiva && botaoPressionado && millis() - tempoEspera >= INTERVALO_MSG)
    {
        Serial.println("Botão pressionado, mas não há carro na posição.");
        Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
        tempoEspera = millis();
    }
    // Carro aguardando botão
    else if (entradaAtiva && !botaoPressionado && millis() - tempoEspera3 >= INTERVALO_MSG)
    {
        Serial.println("há carro na posição, aguardando pressionamento do botão...");
        Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
        tempoEspera3 = millis();
    }
    // Sistema ocioso
    else if (!entradaAtiva && !botaoPressionado && millis() - tempoEspera4 >= INTERVALO_MSG)
    {
        Serial.println("Não há carro na posição, sistema em idle (Aguardando Veiculos)");
        Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
        tempoEspera4 = millis();
    }
}
