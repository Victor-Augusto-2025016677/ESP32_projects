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
        - resistores
            - 300 ohms - 3
            - 10k ohms - 3

    Links:
        - Wokwi Projeto: https://wokwi.com/projects/433713998863293441
        - GitHub: https://github.com/Victor-Augusto-2025016677/ESP32_projects.git
*/

//#include <Arduino.h> // Somente necessário para o platformio, caso contrario, não é necessário, pois o Arduino IDE já inclui essa biblioteca por padrão.
#include <NTPClient.h> // Cliente NTP para sincronização de tempo
#include <WiFi.h> // Biblioteca para conexão Wi-Fi
#include <WiFiUdp.h> // Biblioteca para comunicação UDP, necessária para o NTPClient
#include <time.h> // Biblioteca para manipulação de tempo
#include <EEPROM.h> // Biblioteca para manipulação da memória EEPROM

#define EEPROM_SIZE 4     // Espaço necessário para um int (4 bytes)
#define ADDR_CONTADOR 0   // Endereço onde o contador será salvo

// Definição das credenciais de Wi-Fi
const char *ssid     = "Wifi2"; // Nome da rede Wi-Fi (SSID)
const char *password = "01010101"; // Senha da rede Wi-Fi

/*
    Para que a ESP32 conecte-se a esta rede Wi-Fi, crie um hotspot com as seguintes configurações:
        - Nome da rede: Wifi2
        - Senha: 01010101
        - Tipo de segurança: WPA2-PSK
        - Banda: 2.4GHz
*/

// Inicialização do cliente NTP para obter data/hora atual
WiFiUDP ntpUDP; // Cria uma instância UDP para o NTP
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600); // Cliente NTP configurado para UTC-3 (Brasília)

// Variáveis para armazenar data e hora atuais
String dataAtual; // Armazena a data no formato DD/MM/AAAA
String horaAtual; // Armazena a hora no formato HH:MM:SS

//controle de reset diário
String ultimaDataReset = ""; // Armazena a data do último reset do contador

// Definição dos pinos utilizados
const byte botao = 16; // Pino do botão de impressão do ticket
const byte sensorEntrada = 21; // Pino do sensor de presença na entrada
const byte sensorSaida = 18; // Pino do sensor de presença na saída
const byte ledVerde = 27; // Pino do LED verde (cancela aberta)
const byte ledVermelho = 26; // Pino do LED vermelho (cancela fechada)
const byte ledImpressao = 13; // Pino do LED de indicação de impressão
const byte buzzer = 12; // Pino do buzzer para avisos sonoros

// Variáveis para controle de tempo de espera e timeout
unsigned long tempoEspera = 0; // Controle de intervalo para mensagens de status gerais
unsigned long tempoEspera1 = 0; // Controle de intervalo para mensagens de status na entrada
unsigned long tempoEspera2 = 0; // Controle de intervalo para mensagens de status na saída
unsigned long tempoEspera3 = 0; // Controle de intervalo para mensagens de status: carro aguardando botão
unsigned long tempoEspera4 = 0; // Controle de intervalo para mensagens de status: sistema ocioso
bool timeout1 = false;          // Indica se houve timeout na entrada
bool timeout2 = false;          // Indica se houve timeout na saída
bool exec1 = false; // Variável para controle de execução do reset diário

// Constantes de tempo (em milissegundos)
const unsigned long TIMEOUT_ENTRADA = 30000; // Timeout da entrada (30 segundos)
const unsigned long TIMEOUT_SAIDA = 30000; // Timeout da saída (30 segundos)
const unsigned long DELAY_IMPRESSAO = 1500;   // Duração da impressão do ticket (1,5s, usada 2x = 3s)
const unsigned long DELAY_FECHAMENTO = 2000;  // Tempo de espera antes de fechar a cancela (2s)
const unsigned long DELAY_CANCELA = 500;   // Tempo de abertura/fechamento da cancela (0,5s)
const unsigned long INTERVALO_MSG = 2000;  // Intervalo entre mensagens de status (2s)

// Parâmetros do buzzer
const int SEGUNDOS_AVISO_BUZZER = 5;         // Quantidade de avisos sonoros antes de fechar a cancela
const unsigned long delaybuzzer = 500;       // Duração de cada aviso sonoro (0,5s)

// Variáveis do contador de carros e controle de reset diário
unsigned int NumeroCarros = 0;               // Contador de carros que entraram no estacionamento

bool cancelaAberta = false;                  // Indica o estado atual da cancela

// ==================== FUNÇÕES AUXILIARES ====================

// Atualiza as variáveis dataAtual e horaAtual com base no NTP
void atualizarDataHora() {
    if (WiFi.status() == WL_CONNECTED) { // Verifica se o Wi-Fi está conectado
        timeClient.update(); // Sincroniza o cliente NTP com o servidor
        time_t rawTime = timeClient.getEpochTime(); // Obtém o tempo em formato Epoch (segundos desde 1970)
        struct tm *timeInfo = localtime(&rawTime); // Converte o tempo Epoch para uma estrutura de tempo local

        char dataBuffer[11]; // Buffer para armazenar a string da data
        char horaBuffer[9];  // Buffer para armazenar a string da hora
        strftime(dataBuffer, sizeof(dataBuffer), "%d/%m/%Y", timeInfo); // Formata a data para o padrão DD/MM/AAAA
        strftime(horaBuffer, sizeof(horaBuffer), "%H:%M:%S", timeInfo); // Formata a hora para o padrão HH:MM:SS

        dataAtual = String(dataBuffer); // Converte o buffer de data para String
        horaAtual = String(horaBuffer); // Converte o buffer de hora para String

        if (exec1 == false) { // Executa apenas uma vez no início para definir a data de referência
            ultimaDataReset = dataAtual; // Define a data do último reset como a data atual
            exec1 = true; // Impede que este bloco seja executado novamente
        }
    } else { // Caso não haja conexão Wi-Fi
        // Usa a data e hora de compilação como fallback
        struct tm timeInfo = {0}; // Inicializa a estrutura de tempo
        strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &timeInfo); // Converte a data/hora de compilação

        char dataBuffer[11]; // Buffer para armazenar a string da data
        char horaBuffer[9]; // Buffer para armazenar a string da hora
        strftime(dataBuffer, sizeof(dataBuffer), "%d/%m/%Y", &timeInfo); // Formata a data
        strftime(horaBuffer, sizeof(horaBuffer), "%H:%M:%S", &timeInfo); // Formata a hora

        dataAtual = String(dataBuffer); // Converte o buffer de data para String
        horaAtual = String(horaBuffer); // Converte o buffer de hora para String

        if (exec1 == false) { // Executa apenas uma vez no início
            ultimaDataReset = dataAtual; // Define a data de referência para o reset diário
            exec1 = true; // Impede que este bloco seja executado novamente
        }
    }
}

// Fecha a cancela, atualiza LEDs e status
void FecharCancela() {
    digitalWrite(ledImpressao, LOW);   // Desliga o LED de impressão
    digitalWrite(ledVerde, LOW);       // Desliga o LED verde
    digitalWrite(ledVermelho, HIGH);   // Liga o LED vermelho
    Serial.println("Cancela fechada.\n");
    delay(DELAY_CANCELA);              // Aguarda o tempo de fechamento
    cancelaAberta = false;             // Atualiza o estado da cancela
}

// Emite avisos sonoros antes de fechar a cancela (usado em timeout ou fraude)
void aviso() {
    Serial.println("A cancela irá fechar em " + String(SEGUNDOS_AVISO_BUZZER) + " segundos\n");
    for (int i = 0; i < SEGUNDOS_AVISO_BUZZER; i++) { // Loop para emitir bips sonoros
        digitalWrite(buzzer, HIGH);    // Liga o buzzer
        delay(delaybuzzer);            // Espera 0,5s
        digitalWrite(buzzer, LOW);     // Desliga o buzzer
        delay(delaybuzzer);            // Espera 0,5s
    }
    FecharCancela(); // Chama a função para fechar a cancela
}

// Simula a impressão do ticket de entrada, incrementa o contador e mostra informações detalhadas
void ImprimirTicket() {
    Serial.println("Imprimindo ticket...\n");
    digitalWrite(ledImpressao, HIGH);  // Acende o LED de impressão
    delay(DELAY_IMPRESSAO);            // Simula tempo de impressão
    atualizarDataHora();               // Atualiza data/hora antes de imprimir
    Serial.println("| ==================== |");
    Serial.println("| Ticket de Entrada");
    Serial.println("| Data: " + dataAtual);
    Serial.println("| Hora: " + horaAtual);
    Serial.println("| Carro N°: " + String(++NumeroCarros));
    Serial.println("| ==================== |\n");
    delay(DELAY_IMPRESSAO);            // Simula tempo de impressão (total 3s)
    digitalWrite(ledImpressao, LOW);   // Apaga o LED de impressão
    Serial.println("Ticket impresso.\n");
}

// Abre a cancela, atualiza LEDs e status
void AbrirCancela() {
    Serial.println("Abrindo cancela...\n");
    digitalWrite(ledImpressao, LOW);   // Garante que o LED de impressão está desligado
    digitalWrite(ledVermelho, LOW);    // Desliga o LED vermelho
    digitalWrite(ledVerde, HIGH);      // Liga o LED verde
    Serial.println("Cancela aberta.\n");
    delay(DELAY_CANCELA);              // Aguarda o tempo de abertura
    cancelaAberta = true;              // Atualiza o estado da cancela
}

// Garante que a cancela não fique aberta por muito tempo na entrada (timeout de segurança)
void SegurancaEntrada() {
    timeout1 = false;                  // Reseta o timeout para a entrada
    tempoEspera1 = millis();           // Reseta o tempo para mensagens de status
    unsigned long tempoInicio = millis(); // Marca o início da operação

    while (digitalRead(sensorEntrada)) { // Enquanto há um carro na entrada
        if (millis() - tempoInicio >= TIMEOUT_ENTRADA) { // Se excedeu o tempo limite
            Serial.println("Tempo limite excedido. Cancelando operação.\n");
            aviso();                   // Fecha a cancela com aviso sonoro
            timeout1 = true; // Sinaliza que ocorreu um timeout
            break; // Sai do laço while
        }
        if (millis() - tempoEspera1 >= INTERVALO_MSG) { // Mensagem periódica de status
            Serial.println("Carro ainda posicionado na entrada...\n");
            tempoEspera1 = millis(); // Atualiza o tempo para a próxima mensagem
        }
    }

    if (!digitalRead(sensorEntrada)) { // Se o carro saiu da entrada
        Serial.println("Carro saiu da entrada.\n");
    }
}

// Garante a segurança na saída, evitando passagem consecutiva e timeout
void SegurancaSaida() {
    timeout2 = false;                  // Reseta o timeout para a saída
    tempoEspera2 = millis();           // Reseta o tempo para mensagens de status
    unsigned long tempoInicio1 = millis(); // Marca o início da operação

    while (!digitalRead(sensorSaida) && !timeout1) { // Enquanto o carro não saiu e não houve timeout na entrada
        if (millis() - tempoInicio1 >= TIMEOUT_SAIDA) { // Se excedeu o tempo limite
            Serial.println("Tempo limite excedido. Cancelando operação.\n");
            aviso();                   // Fecha a cancela com aviso sonoro
            timeout2 = true; // Sinaliza que ocorreu um timeout
            break; // Sai do laço while
        }
        if (millis() - tempoEspera2 >= INTERVALO_MSG) { // Mensagem periódica de status
            Serial.println("Carro ainda não saiu completamente...\n");
            tempoEspera2 = millis(); // Atualiza o tempo para a próxima mensagem
        }
    }

    // Anti-fraude: detecta tentativa de passagem consecutiva
    if (digitalRead(sensorEntrada) && digitalRead(sensorSaida)) {
        Serial.println("Carro detectado na entrada e saída. Fechando cancela imediatamente para impedir passagem consecutiva sem emissão de ticket.\n");
        FecharCancela(); // Fecha a cancela imediatamente
    } else if (digitalRead(sensorSaida)) { // Carro saiu normalmente
        Serial.println("Carro detectado na saída. Fechando cancela dentro de 2 segundos.\n");
        delay(DELAY_FECHAMENTO); // Aguarda um tempo antes de fechar
        FecharCancela(); // Fecha a cancela
    }
}

// ==================== SETUP ====================

// Inicializa o sistema, define pinos, conecta ao Wi-Fi e sincroniza o relógio
void setup() {

    EEPROM.begin(EEPROM_SIZE); // Inicializa a EEPROM com o tamanho definido
    EEPROM.get(ADDR_CONTADOR, NumeroCarros); // Lê o último valor salvo do contador de carros da EEPROM

    Serial.begin(115200); // Inicializa a comunicação serial com baud rate de 115200

    // Define os pinos como entrada ou saída conforme o hardware
    pinMode(botao, INPUT); // Pino do botão como entrada
    pinMode(sensorEntrada, INPUT); // Pino do sensor de entrada como entrada
    pinMode(sensorSaida, INPUT); // Pino do sensor de saída como entrada
    pinMode(ledVerde, OUTPUT); // Pino do LED verde como saída
    pinMode(ledVermelho, OUTPUT); // Pino do LED vermelho como saída
    pinMode(ledImpressao, OUTPUT); // Pino do LED de impressão como saída
    pinMode(buzzer, OUTPUT); // Pino do buzzer como saída

    // Estado inicial: cancela fechada, LEDs ajustados
    digitalWrite(ledImpressao, LOW); // Garante que LED de impressão comece desligado
    digitalWrite(ledVerde, LOW); // Garante que LED verde comece desligado
    digitalWrite(ledVermelho, HIGH); // Liga o LED vermelho para indicar cancela fechada
    cancelaAberta = false; // Define o estado inicial da cancela como fechada

    Serial.println("Sistema de Controle de Cancela Iniciado.\n");

    // Conexão com a rede Wi-Fi
    WiFi.begin(ssid, password); // Inicia a tentativa de conexão Wi-Fi
    int a = 0; // Contador para limitar o tempo de tentativa de conexão
    while (a < 20) { // Tenta conectar por aproximadamente 10 segundos
    while (WiFi.status() != WL_CONNECTED) {
        delay(500); // Aguarda 0,5 segundo
        Serial.print("."); // Imprime um ponto para indicar tentativa
        a++;
    }
    }

    if (WiFi.status() == WL_CONNECTED) { // Se a conexão foi bem-sucedida
        Serial.println("WiFi conectado!\n");
        // Inicializa o cliente NTP
        timeClient.begin(); // Inicia o cliente NTP para sincronização de tempo
    }

}

// ==================== LOOP PRINCIPAL ====================

// Loop principal: gerencia o fluxo de veículos, impressão de tickets e segurança
void loop() {
    atualizarDataHora(); // Atualiza data e hora atuais a cada ciclo
    bool entradaAtiva = digitalRead(sensorEntrada); // Lê o estado do sensor de entrada
    bool saidaAtiva = digitalRead(sensorSaida);   // Lê o estado do sensor de saída
    bool botaoPressionado = digitalRead(botao);       // Lê o estado do botão de solicitação

    // Reset diário do contador de carros APENAS quando o dia virar
    if (dataAtual != ultimaDataReset) {
        NumeroCarros = 0; // Zera a variável do contador
        EEPROM.put(ADDR_CONTADOR, 0); // Salva o valor zero na EEPROM
        EEPROM.commit(); // Confirma a gravação na EEPROM
        ultimaDataReset = dataAtual; // Atualiza a data do último reset
        Serial.println("Contador de carros resetado: novo dia detectado.\n");
    }

    // Caso 1: Carro na entrada e botão pressionado - inicia sequência principal
    if (entradaAtiva && botaoPressionado) {
        Serial.println("Botão pressionado.\n");
        ImprimirTicket();       // Simula impressão do ticket
        AbrirCancela();         // Abre a cancela
        SegurancaEntrada();     // Garante segurança na entrada (timeout)
        SegurancaSaida();       // Garante segurança na saída (timeout e anti-fraude)

        // Se houve timeout, desfaz o incremento do contador
        if (timeout1 || timeout2) {
            Serial.println("Operação cancelada devido ao timeout. Contador de carros não incrementado.\n");
            if (NumeroCarros > 0) { // Garante que o contador não fique negativo
                NumeroCarros--; // Decrementa o contador para corrigir a contagem
            }
        } else {
            // Se a operação foi bem-sucedida, salva o novo valor na EEPROM
            EEPROM.put(ADDR_CONTADOR, NumeroCarros); // Salva o valor atualizado na EEPROM
            EEPROM.commit(); // Confirma a gravação
            Serial.println("Carro registrado com sucesso.\n");
            Serial.println("Sistema pronto para novo veículo.\n");
        }
    }
    // Caso 2: Botão pressionado sem carro na entrada
    else if (!entradaAtiva && botaoPressionado && millis() - tempoEspera >= INTERVALO_MSG) {
        Serial.println("Botão pressionado, mas não há carro na posição.");
        Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
        tempoEspera = millis(); // Reseta o timer da mensagem para evitar spam
    }
    // Caso 3: Carro na entrada, aguardando pressionamento do botão
    else if (entradaAtiva && !botaoPressionado && millis() - tempoEspera3 >= INTERVALO_MSG) {
        Serial.println("Há carro na posição, aguardando pressionamento do botão...");
        Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
        tempoEspera3 = millis(); // Reseta o timer da mensagem para evitar spam
    }
    // Caso 4: Sistema ocioso, aguardando veículos
    else if (!entradaAtiva && !botaoPressionado && millis() - tempoEspera4 >= INTERVALO_MSG) {
        Serial.println("Não há carro na posição, sistema em idle (Aguardando Veículos)");
        Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
        tempoEspera4 = millis(); // Reseta o timer da mensagem para evitar spam
    }
}