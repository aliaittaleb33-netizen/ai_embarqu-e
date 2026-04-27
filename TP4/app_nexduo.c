/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_netxduo.c
  * @author  MCD Application Team
  * @brief   NetXDuo applicative file
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_netxduo.h"

/* Private includes ----------------------------------------------------------*/
#include "nxd_dhcp_client.h"
/* USER CODE BEGIN Includes */
#include "nxd_dns.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
#define DEFAULT_MESSAGE "GET / HTTP/1.1\r\nHost: api.thingspeak.com\r\nConnection: close\r\n\r\n"
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TX_THREAD      NxAppThread;
NX_PACKET_POOL NxAppPool;
NX_IP          NetXDuoEthIpInstance;
TX_SEMAPHORE   DHCPSemaphore;
NX_DHCP        DHCPClient;
/* USER CODE BEGIN PV */
TX_THREAD AppTCPThread;
TX_THREAD AppLinkThread;

NX_TCP_SOCKET TCPSocket;
NX_DNS        DnsClient;

ULONG         IpAddress;
ULONG         NetMask;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID nx_app_thread_entry (ULONG thread_input);
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr);
/* USER CODE BEGIN PFP */
static VOID App_TCP_Thread_Entry(ULONG thread_input);
static VOID App_Link_Thread_Entry(ULONG thread_input);
/* USER CODE END PFP */

/**
  * @brief  Application NetXDuo Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  CHAR *pointer;

  /* USER CODE BEGIN MX_NetXDuo_MEM_POOL */
  /* USER CODE END MX_NetXDuo_MEM_POOL */

  /* USER CODE BEGIN 0 */
  printf("Nx_TCP_Echo_Client application started..\n");
  /* USER CODE END 0 */

  /* Initialize the NetXDuo system. */
  nx_system_initialize();

  /* Allocate the memory for packet_pool */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the Packet pool */
  ret = nx_packet_pool_create(&NxAppPool, "NetXDuo App Pool", DEFAULT_PAYLOAD_SIZE, pointer, NX_APP_PACKET_POOL_SIZE);
  if (ret != NX_SUCCESS)
  {
    return NX_POOL_ERROR;
  }

  /* Allocate the memory for Ip_Instance */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, Nx_IP_INSTANCE_THREAD_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the main NX_IP instance */
  ret = nx_ip_create(&NetXDuoEthIpInstance, "NetX Ip instance",
                     NX_APP_DEFAULT_IP_ADDRESS, NX_APP_DEFAULT_NET_MASK,
                     &NxAppPool, nx_stm32_eth_driver,
                     pointer, Nx_IP_INSTANCE_THREAD_SIZE,
                     NX_APP_INSTANCE_PRIORITY);
  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Allocate the memory for ARP */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, DEFAULT_ARP_CACHE_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Enable the ARP protocol */
  ret = nx_arp_enable(&NetXDuoEthIpInstance, (VOID *)pointer, DEFAULT_ARP_CACHE_SIZE);
  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable the ICMP */
  ret = nx_icmp_enable(&NetXDuoEthIpInstance);
  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable TCP Protocol */

  /* Allocate the memory for TCP thread */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the TCP thread */
  ret = tx_thread_create(&AppTCPThread, "App TCP Thread", App_TCP_Thread_Entry,
                         0, pointer, NX_APP_THREAD_STACK_SIZE,
                         NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY,
                         TX_NO_TIME_SLICE, TX_DONT_START);
  if (ret != TX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  ret = nx_tcp_enable(&NetXDuoEthIpInstance);
  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable the UDP protocol (required for DHCP and DNS) */
  ret = nx_udp_enable(&NetXDuoEthIpInstance);
  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* USER CODE BEGIN DNS_Init */

  /* USER CODE END DNS_Init */

  /* Allocate the memory for main thread */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the main thread */
  ret = tx_thread_create(&NxAppThread, "NetXDuo App thread", nx_app_thread_entry,
                         0, pointer, NX_APP_THREAD_STACK_SIZE,
                         NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY,
                         TX_NO_TIME_SLICE, TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* Create the DHCP client */
  ret = nx_dhcp_create(&DHCPClient, &NetXDuoEthIpInstance, "DHCP Client");
  if (ret != NX_SUCCESS)
  {
    return NX_DHCP_ERROR;
  }

  /* Set DHCP notification callback */
  ret = tx_semaphore_create(&DHCPSemaphore, "DHCP Semaphore", 0);
  if (ret != NX_SUCCESS)
  {
    return NX_DHCP_ERROR;
  }

  /* USER CODE BEGIN MX_NetXDuo_Init */

  /* Allocate the memory for Link thread */
  ret = tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT);
  if (ret != TX_SUCCESS)
   {
     printf("ERREUR CRITIQUE : Plus de memoire RAM (tx_byte_allocate echoue avec code 0x%02X)\n", ret);
     return TX_POOL_ERROR;
   }

  /* Create the Link thread */
  ret = tx_thread_create(&AppLinkThread, "App Link Thread", App_Link_Thread_Entry,
                           0, pointer, NX_APP_THREAD_STACK_SIZE,
                           LINK_PRIORITY, LINK_PRIORITY,
                           TX_NO_TIME_SLICE, TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* USER CODE END MX_NetXDuo_Init */

  return ret;
}

/**
  * @brief  ip address change callback.
  */
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr)
{
  /* USER CODE BEGIN ip_address_change_notify_callback */
  if (nx_ip_address_get(&NetXDuoEthIpInstance, &IpAddress, &NetMask) != NX_SUCCESS)
  {
    Error_Handler();
  }
  if (IpAddress != NULL_ADDRESS)
  {
    tx_semaphore_put(&DHCPSemaphore);
  }
  /* USER CODE END ip_address_change_notify_callback */
}

/**
  * @brief  Main thread entry.
  */
static VOID nx_app_thread_entry (ULONG thread_input)
{
  UINT ret = NX_SUCCESS;

  /* Register the IP address change callback */
  ret = nx_ip_address_change_notify(&NetXDuoEthIpInstance,
                                    ip_address_change_notify_callback, NULL);
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  /* Start the DHCP client */
  ret = nx_dhcp_start(&DHCPClient);
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  printf("Looking for DHCP server ..\n");

  /* Wait until an IP address is ready */
  if (tx_semaphore_get(&DHCPSemaphore, TX_WAIT_FOREVER) != TX_SUCCESS)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Nx_App_Thread_Entry 2 */

  PRINT_IP_ADDRESS(IpAddress);

  /* Ajouter le serveur DNS obtenu via DHCP (ou garder Google DNS) */
  /* On peut aussi ajouter la gateway comme DNS secondaire */
  printf("DNS configure : 8.8.8.8\n");

  /* The network is ready, start the TCP thread */
  tx_thread_resume(&AppTCPThread);

  tx_thread_relinquish();
  return;

  /* USER CODE END Nx_App_Thread_Entry 2 */
}

/* USER CODE BEGIN 1 */

/**
  * @brief  TCP thread entry — teste la résolution DNS puis connexion TCP.
  */
/**
  * @brief  TCP thread entry — teste la résolution DNS puis connexion TCP.
  */
static VOID App_TCP_Thread_Entry(ULONG thread_input)
{
    UINT ret;
    ULONG server_ip = 0;

    NX_PACKET *data_packet;
    NX_PACKET *server_packet;
    ULONG bytes_read;
    UCHAR data_buffer[512];

    /* =======================================================
     * INITIALISATION DU DNS DANS LE THREAD
     * ======================================================= */
    printf("Initialisation du DNS dans le thread...\r\n");

    ret = nx_dns_create(&DnsClient, &NetXDuoEthIpInstance, (UCHAR *)"DNS Client");
    if (ret == NX_SUCCESS)
    {
        nx_dns_packet_pool_set(&DnsClient, &NxAppPool);
        nx_dns_server_add(&DnsClient, IP_ADDRESS(8, 8, 8, 8));
        printf("DNS Init terminee avec succes.\r\n");
    }
    else
    {
        printf("Erreur creation DNS : 0x%02X\r\n", ret);
    }

    /* -------------------------------------------------------
     * ÉTAPE 1 : Résolution DNS de api.thingspeak.com
     * ------------------------------------------------------- */
    printf("Resolution DNS de api.thingspeak.com ...\r\n");

    ret = nx_dns_host_by_name_get(&DnsClient,
                                  (UCHAR *)"api.thingspeak.com",
                                  &server_ip,
                                  10 * NX_IP_PERIODIC_RATE);

    if (ret != NX_SUCCESS)
    {
        printf("Echec resolution DNS ! (code: %u)\r\n", ret);
        /* On garde l'IP fixe en fallback */
        server_ip = IP_ADDRESS(184, 106, 153, 149);
        printf("Utilisation IP fixe : 184.106.153.149\r\n");
    }
    else
    {
        printf("DNS OK ! IP ThingSpeak : %lu.%lu.%lu.%lu\r\n",
               (server_ip >> 24) & 0xFF,
               (server_ip >> 16) & 0xFF,
               (server_ip >>  8) & 0xFF,
                server_ip        & 0xFF);
    }

    /* =======================================================
     * BOUCLE PRINCIPALE D'ENVOI CONTINU
     * ======================================================= */
    static float fake_temperature = 28.0;
    static float fake_humidity = 45.0;
    static float fake_pressure = 1010.0;

    while (1)
    {
        /* -------------------------------------------------------
         * ÉTAPE 2 : Création de la socket TCP et Connexion
         * ------------------------------------------------------- */
        ret = nx_tcp_socket_create(&NetXDuoEthIpInstance, &TCPSocket,
                                   "TCP Server Socket",
                                   NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                                   NX_IP_TIME_TO_LIVE, WINDOW_SIZE,
                                   NX_NULL, NX_NULL);
        if (ret != NX_SUCCESS)
        {
            tx_thread_sleep(NX_IP_PERIODIC_RATE * 5);
            continue;
        }

        ret = nx_tcp_client_socket_bind(&TCPSocket, DEFAULT_PORT, NX_WAIT_FOREVER);
        if (ret != NX_SUCCESS)
        {
            nx_tcp_socket_delete(&TCPSocket);
            tx_thread_sleep(NX_IP_PERIODIC_RATE * 5);
            continue;
        }

        printf("\nConnexion au serveur TCP...\r\n");
        ret = nx_tcp_client_socket_connect(&TCPSocket, server_ip, 80, NX_WAIT_FOREVER);

        if (ret != NX_SUCCESS)
        {
            printf("Erreur de connexion TCP ! (Code: 0x%02X)\r\n", ret);
            nx_tcp_client_socket_unbind(&TCPSocket);
            nx_tcp_socket_delete(&TCPSocket);
            tx_thread_sleep(NX_IP_PERIODIC_RATE * 5);
            continue;
        }

        printf("Connecte au serveur TCP !\r\n");

        /* On fait varier les valeurs capteurs */
        fake_temperature = fake_temperature + 0.5;
        fake_humidity = fake_humidity + 3.0;
        fake_pressure = fake_pressure + 2.5;

        if (fake_temperature > 30.0) fake_temperature = 28.0;
        if (fake_humidity > 80.0) fake_humidity = 45.0;
        if (fake_pressure > 1030.0) fake_pressure = 990.0;

        /* =======================================================
         * EXÉCUTION DE L'IA EMBARQUÉE (INFERENCE)
         * ======================================================= */
        float ai_input[3] = {fake_temperature, fake_humidity, fake_pressure};
        float ai_output[2] = {0.0, 0.0};

        /* Décommenter la ligne ci-dessous uniquement si la mémoire RAM (Stack/Heap) a été augmentée */
        // Run_Inference(ai_input, ai_output);

        /* Prédiction simulée pour la démo */
        float proba_pluie = 85.0;
        /* ======================================================= */

        char http_request[256];
        TX_MEMSET(data_buffer, '\0', sizeof(data_buffer));

        ret = nx_packet_allocate(&NxAppPool, &data_packet, NX_TCP_PACKET, TX_WAIT_FOREVER);
        if (ret == NX_SUCCESS)
        {
            /* Formatage avec conversion en entiers (%d) pour ThingSpeak */
            sprintf(http_request,
                    "GET /update?api_key=K2KZJ5HE0O2WWOLZ&field1=%d&field2=%d&field3=%d&field4=%d HTTP/1.1\r\n"
                    "Host: api.thingspeak.com\r\n"
                    "Connection: close\r\n\r\n",
                    (int)fake_temperature, (int)fake_humidity, (int)fake_pressure, (int)proba_pluie);

            ret = nx_packet_data_append(data_packet, (VOID *)http_request, strlen(http_request), &NxAppPool, TX_WAIT_FOREVER);

            if (ret == NX_SUCCESS)
            {
                ret = nx_tcp_socket_send(&TCPSocket, data_packet, DEFAULT_TIMEOUT);
                if (ret == NX_SUCCESS)
                {
                    ret = nx_tcp_socket_receive(&TCPSocket, &server_packet, DEFAULT_TIMEOUT);
                    if (ret == NX_SUCCESS)
                    {
                        ULONG source_ip;
                        UINT  source_port;
                        nx_udp_source_extract(server_packet, &source_ip, &source_port);
                        nx_packet_data_retrieve(server_packet, data_buffer, &bytes_read);
                        PRINT_DATA(source_ip, source_port, data_buffer);
                        nx_packet_release(server_packet);
                        HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
                        printf("\tSUCCESS : Donnees + Prediction IA envoyees !\r\n");
                    }
                }
            }
            if (ret != NX_SUCCESS) nx_packet_release(data_packet);
        }

        /* -------------------------------------------------------
         * ÉTAPE 3 : Déconnexion et Attente
         * ------------------------------------------------------- */
        nx_tcp_socket_disconnect(&TCPSocket, DEFAULT_TIMEOUT);
        nx_tcp_client_socket_unbind(&TCPSocket);
        nx_tcp_socket_delete(&TCPSocket);

        printf("Attente 20 secondes avant le prochain envoi...\r\n");
        tx_thread_sleep(NX_IP_PERIODIC_RATE * 20);
    }
}
/**
  * @brief  Link thread entry.
  */
static VOID App_Link_Thread_Entry(ULONG thread_input)
{
  ULONG actual_status;
  UINT linkdown = 0, status;

  while (1)
  {
    status = nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0,
                                          NX_IP_LINK_ENABLED,
                                          &actual_status, 10);
    if (status == NX_SUCCESS)
    {
      if (linkdown == 1)
      {
        linkdown = 0;
        printf("The network cable is connected.\n");

        nx_ip_driver_direct_command(&NetXDuoEthIpInstance,
                                    NX_LINK_ENABLE, &actual_status);

        status = nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0,
                                              NX_IP_ADDRESS_RESOLVED,
                                              &actual_status, 10);
        if (status == NX_SUCCESS)
        {
          nx_dhcp_stop(&DHCPClient);
          nx_dhcp_reinitialize(&DHCPClient);
          nx_dhcp_start(&DHCPClient);

          if (tx_semaphore_get(&DHCPSemaphore, TX_WAIT_FOREVER) != TX_SUCCESS)
          {
            Error_Handler();
          }
          PRINT_IP_ADDRESS(IpAddress);
        }
        else
        {
          nx_dhcp_client_update_time_remaining(&DHCPClient, 0);
        }
      }
    }
    else
    {
      if (linkdown == 0)
      {
        linkdown = 1;
        printf("The network cable is not connected.\n");
        nx_ip_driver_direct_command(&NetXDuoEthIpInstance,
                                    NX_LINK_DISABLE, &actual_status);
      }
    }

    tx_thread_sleep(NX_APP_CABLE_CONNECTION_CHECK_PERIOD);
  }
}

/* USER CODE END 1 */

