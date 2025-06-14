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
        - resistores
            - 300 ohms - 3
            - 10k ohms - 3

    Links:
        -Wokwi Projeto: https://wokwi.com/projects/433713998863293441
        -GitHub: https://github.com/Victor-Augusto-2025016677/ESP32_projects.git
*/

//#include <Arduino.h> //Somente necessário para o platformio, caso contrario, não é necessário, pois o Arduino IDE já inclui essa biblioteca por padrão.
#include <NTPClient.h> //Cliente NTP para sincronização de tempo
#include <WiFi.h> //Biblioteca para conexão Wi-Fi
#include <WiFiUdp.h> //Biblioteca para comunicação UDP, necessária para o NTPClient
#include <time.h> // Biblioteca para manipulação de tempo

// Definição das credenciais de Wi-Fi
const char *ssid     = "Wifi2";
const char *password = "01010101";

/*
Para que a esp conecte-se a esta rede wifi, crie um hotspot com as seguintes configurações:

    - Nome da rede: Wifi2
    - Senha: 01010101
    - Tipo de segurança: WPA2-PSK
    - Banda: 2.4GHz

*/

// Inicialização do cliente NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600); // UTC-3

String dataAtual; // Variável para armazenar a data atual
String horaAtual; // Variável para armazenar a hora atual

// Definição dos pinos utilizados
const byte botao = 25;
const byte sensorEntrada = 33;
const byte sensorSaida = 34;
const byte ledVerde = 27;
const byte ledVermelho = 26;
const byte ledImpressao = 14;
const byte buzzer = 12;

// Variáveis para controle de tempo de espera para o timeout
unsigned long tempoEspera = 0; // Variável para armazenar o tempo de espera, inicia zerada. 
unsigned long tempoEspera1 = 0;
unsigned long tempoEspera2 = 0;
unsigned long tempoEspera3 = 0;
unsigned long tempoEspera4 = 0;
bool timeout1 = false; // Variável para controle de timeout na entrada
bool timeout2 = false; // Variável para controle de timeout na saida

//Variaveis de tempo
const unsigned long TIMEOUT_ENTRADA = 30000; //timeout da entrada, em milissegundos (30 segundos)
const unsigned long TIMEOUT_SAIDA = 30000; //timeout da saida, em milissegundos (30 segundos)
const unsigned long DELAY_IMPRESSAO = 1500; // Duração da impressão do ticket, em milissegundos, é utilizada 2 vezes, logo, 3 segundos de impressão
const unsigned long DELAY_FECHAMENTO = 2000; // Duração do fechamento da cancela, em milissegundos (2 segundos)
const unsigned long DELAY_CANCELA = 500; // Duração da abertura/fechamento da cancela, em milissegundos (0,5 segundo)
const unsigned long INTERVALO_MSG = 2000; // Intervalo de tempo entre mensagens de status, em milissegundos (2 segundos)

//variaveis buzzer
const int SEGUNDOS_AVISO_BUZZER = 5; //Quantidade de vezes que o buzzer irá tocar, e duração total em seugundos
const unsigned long delaybuzzer = 500; //Atenção, a função de ativação do buzzer, assume que esse valor é 0.5s (500), ou seja, caso altere, a constante anterior, vai ser inexata. 

//Outras variaveis
unsigned int NumeroCarros = 0; // Contador de carros que entraram no estacionamento
unsigned long horaultimoReset = 0; // Armazena o horário do último reset do contador
const unsigned long intervaloReset = 24UL * 60UL * 60UL * 1000UL; // 24 horas em milissegundos
bool cancelaAberta = false; // Variável para controle do estado da cancela

//Inicio das funções

void atualizarDataHora() 
{
  timeClient.update(); // Atualiza o cliente NTP para obter a hora atual

  time_t rawTime = timeClient.getEpochTime(); // Obtém o tempo atual em segundos desde 1 de janeiro de 1970
  struct tm * timeInfo = localtime(&rawTime); // Converte o tempo bruto para a estrutura tm, que contém informações sobre data e hora

  char dataBuffer[11]; // Formato: dd/mm/yyyy
  char horaBuffer[9]; // Formato: hh:mm:ss
  strftime(dataBuffer, sizeof(dataBuffer), "%d/%m/%Y", timeInfo); // Formata a data
  strftime(horaBuffer, sizeof(horaBuffer), "%H:%M:%S", timeInfo); // Formata a hora

  dataAtual = String(dataBuffer); // Converte o buffer de data para String
  horaAtual = String(horaBuffer); // Converte o buffer de hora para String
}

void FecharCancela() // Esta função é usada para fechar a cancela
{
    digitalWrite(ledImpressao, LOW); // Desliga o LED de impressão
    digitalWrite(ledVerde, LOW); // Desliga o LED verde
    digitalWrite(ledVermelho, HIGH); // Liga o LED vermelho
    Serial.println("Cancela fechada.\n"); // Mensagem de confirmação de fechamento da cancela
    delay(DELAY_CANCELA); // Mantém a cancela fechada por 0,5 segundo
    cancelaAberta = false; // Define o estado da cancela como fechada
}

void aviso() //Função para fechar a cancela com avisos sonoros. 
{
  Serial.println("A cancela irá fechar em " + String(SEGUNDOS_AVISO_BUZZER) + " segundos\n"); //mensagem de aviso

  for (int i = 0; i < SEGUNDOS_AVISO_BUZZER; i++) 
  {
    digitalWrite(buzzer, HIGH); // Liga o buzzer
    delay(delaybuzzer); // Espera 500ms
    digitalWrite(buzzer, LOW);  // Desliga o buzzer
    delay(delaybuzzer); // Espera 500ms
  }

  FecharCancela();
}

void ImprimirTicket() //Esta função simula a impressão do ticket de entrada
{
    atualizarDataHora();
    Serial.println("Imprimindo ticket...\n"); 
    digitalWrite(ledImpressao, HIGH);
    delay(DELAY_IMPRESSAO);
    Serial.println("| ==================== |"); //delimitação estética do ticket
    Serial.println("| Ticket de Entrada");
    Serial.println("| Data: " + dataAtual); //Data da emissão do ticket
    Serial.println("| Hora: " + horaAtual); //Hora da emissão do ticket
    Serial.println("| Carro N°: " + String(++NumeroCarros)); //Nº do carro
    Serial.println("| ==================== |\n");
    delay(DELAY_IMPRESSAO); //1500ms +1500ms = 3 segundos de impressão, como solicitado 
    digitalWrite(ledImpressao, LOW);
    Serial.println("Ticket impresso.\n"); // Mensagem de confirmação de impressão
}

void AbrirCancela() //Esta função é usada para abrir a cancela
{
    Serial.println("Abrindo cancela...\n");
    digitalWrite(ledImpressao, LOW); // Desliga o LED de impressão
    digitalWrite(ledVermelho, LOW); // Desliga o LED vermelho
    digitalWrite(ledVerde, HIGH); // Liga o LED verde
    Serial.println("Cancela aberta.\n"); // Mensagem de confirmação de abertura da cancela
    delay(DELAY_CANCELA); // Mantém a cancela aberta por 0,5 segundo
    cancelaAberta = true; // Define o estado da cancela como aberta
}

void SegurancaEntrada() // Esta função garante que a cancela não fique aberta por muito tempo na entrada
{
    timeout1 = false; // Reseta o timeout para a entrada
    tempoEspera1 = millis(); //reseta o tempo para evitar a repetição de mensagens
    unsigned long tempoInicio = millis(); // Armazena o tempo de início da operação, para a realização do timeout

    while (digitalRead(sensorEntrada)) //Enquanto há um carro na entrada
    {
        if (millis() - tempoInicio >= TIMEOUT_ENTRADA) // Verifica se o tempo de espera excedeu 30 segundos, caso sim, cancela a operação
        {
            Serial.println("Tempo limite excedido. Cancelando operação.\n"); // Mensagem de cancelamento
            aviso(); // Fecha a cancela, com avisos
            timeout1 = true; // Define o timeout como verdadeiro
            break; //encerra o while
            
        }
        if (millis() - tempoEspera1 >= INTERVALO_MSG) //Função para que a mensagem não seja exibida repetidamente, mas sim a cada 3 segundos
        {
            Serial.println("Carro ainda posicionado na entrada...\n"); //Mensagem de aviso
            tempoEspera1 = millis(); // Atualiza o tempo de espera para a próxima verificação
        }
    }

    if (digitalRead(sensorEntrada) == false) //Se o carro saiu da entrada
    {
        Serial.println("Carro saiu da entrada.\n");
    }

}

void SegurancaSaida() //Esta função, garante que a saida ocorra com segurança, realizando verificações para evitar a passagem consecutiva, e danificar o veiculo.
{
    timeout2 = false; // Reseta o timeout para a saída
    tempoEspera2 = millis();
    unsigned long tempoInicio1 = millis(); //Salva o tempo para a realização do timeout

    while (digitalRead(sensorSaida) == false && timeout1 == false) //Enquanto o carro não sair
    {
        if (millis() - tempoInicio1 >= TIMEOUT_SAIDA) //evento de timeout
        {
            Serial.println("Tempo limite excedido. Cancelando operação.\n"); //Aviso de Cancelamento
            aviso(); // Fecha a cancela, com avisos
            timeout2 = true; // Define o timeout como verdadeiro
            break;
        }
        if (millis() - tempoEspera2 >= INTERVALO_MSG) //para a mensagem não se repetir a cada ciclo de processamento
        {
            Serial.println("Carro ainda não saiu completamente...\n"); //Mensagem de feedback
            tempoEspera2 = millis();
        }
    }

    if (digitalRead(sensorEntrada) && digitalRead(sensorSaida)) //Caso tenha um carro na entrada e saída
    {
        Serial.println("Carro detectado na entrada e saída. Fechando cancela imediatamente para impedir passagem consecutiva sem emissão de ticket.\n");
        FecharCancela();
    }
    else if (digitalRead(sensorSaida)) //Caso tenha um carro na saída
    {
        Serial.println("Carro detectado na saída. Fechando cancela dentro de 2 segundos.\n");
        delay(DELAY_FECHAMENTO);  
        FecharCancela();
    }
}

void setup() //Inicia o setup do sistema
{
    Serial.begin(115200); //Inicia o serial

    //Define os pinos como entrada ou Saida
    pinMode(botao, INPUT);
    pinMode(sensorEntrada, INPUT);
    pinMode(sensorSaida, INPUT);
    pinMode(ledVerde, OUTPUT);
    pinMode(ledVermelho, OUTPUT);
    pinMode(ledImpressao, OUTPUT);
    pinMode(buzzer, OUTPUT);

    //Define o estado inicial como fechado
    digitalWrite(ledImpressao, LOW);
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledVermelho, HIGH);
    cancelaAberta = false;
    
    //Mensagem que o sistema iniciou, infelizmente nunca é vista, pois até abrir o monitor serial, isto já foi executado
    Serial.println("Sistema de Controle de Cancela Iniciado.");

    // Conexão com a rede Wi-Fi
    WiFi.begin(ssid, password);
    while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  timeClient.begin();

}

void loop() //Função de loop continuo
{
    unsigned long atual = millis(); //Salva o tempo do loop atual
    bool entradaAtiva = digitalRead(sensorEntrada); //Le o sensor de entrada
    bool saidaAtiva = digitalRead(sensorSaida); //Le o sensor de saida
    bool botaoPressionado = digitalRead(botao); //Le o botão de inicio

    if (atual - horaultimoReset >= intervaloReset) //Reset a cada 24h do número de carros.
    {
        NumeroCarros = 0;
        horaultimoReset = atual;
        Serial.println("Contador de carros resetado após 24h.");
    }

    if (entradaAtiva && botaoPressionado) //Se há carro na posição de entrada, e o botão foi pressionado, inicia a sequencia de ações
    {
        Serial.println("Botão pressionado.\n"); //Mensagem que o botão foi pressionado
        
        ImprimirTicket(); //Inicia a impressão do ticket

        AbrirCancela(); //Abre a cancela

        SegurancaEntrada(); //Chama a função de segurança da entrada

        SegurancaSaida(); //Chama a função de segurança da saída, o fechamento da cancela é chamado internamente.

        if (timeout1 == true || timeout2 == true) //Se algum timeout foi ativado, não incrementa o contador de carros
        {
            Serial.println("Operação cancelada devido ao timeout. Contador de carros não incrementado.\n");
            if (NumeroCarros > 0) 
            {
                NumeroCarros--;  //subtrai 1 do  contador de carros, pois não houve um registro válido
            }
        }
        else //Se não houve timeout, segue normalmente
        {
            Serial.println("Carro registrado com sucesso.\n");
            Serial.println("Sistema pronto para novo veículo.\n"); //Mensagem que o sistema está pronto para uma nova execução
        }
        
    }

        else if (!entradaAtiva && botaoPressionado && millis() - tempoEspera >= INTERVALO_MSG) //Se não há carro na posição, mas o botão foi pressionado
        {
            Serial.println("Botão pressionado, mas não há carro na posição.");
            Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
            tempoEspera = millis();
        }

            else if (entradaAtiva && !botaoPressionado && millis() - tempoEspera3 >= INTERVALO_MSG) //Se há carro, mas o botão não foi pressionado ainda
            {
                Serial.println("há carro na posição, aguardando pressionamento do botão...");
                Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
                tempoEspera3 = millis();
            }

                else if (!entradaAtiva && !botaoPressionado && millis() - tempoEspera4 >= INTERVALO_MSG) //Se não há carro na posição, e o botão não foi pressionado
                {
                    Serial.println("Não há carro na posição, sistema em idle (Aguardando Veiculos)");
                    Serial.println("Status atual cancela: " + String(cancelaAberta ? " Aberta." : " Fechada.") + "\n");
                    tempoEspera4 = millis();
                }

}

