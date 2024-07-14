//Esta es una biblioteca con una estructura que implementa una tabla hash con Open Addressing
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <time.h>

//Definimos macros (cuando el PC compile, YES lo traduce a 1 y NO a 0... no son variables globales)
#define YES 1
#define NO 0
#define VALID 1
#define NOTVALID 0
#define DELETED NOTVALID
#define LAZY_DELETED -1
#define MAX_64 2147483647
#define FULL 2
#define EMPTY 1
#define UP 1
#define DOWN 0
#define LP 1
#define QP 2
#define DH 3

//En este arreglo se contienen los números primos menores a cada potencia de 2 (hasta 2^16)
const uint32_t HASH_SIZE[] = {5, 23, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749, 65521, 131071, 262139, 524287, 1048573, 2097143, 4194301, 8388593, 16777213, 33554393, 67108859, 134217689, 268435399, 536870909, 1073741789, 2147483647, 4294967291};

//Constante de ADLER
const uint32_t MOD_ADLER = 65521;

//Variable Global para la histéresis
int hist = 0;

int aux = 0;

/*Función generador de llaves*/
uint32_t adler32(unsigned char *data, size_t len) {
    uint32_t a = 1, b = 0;
    size_t index;

    //Process each byte of the data in order
    for (index = 0; index < len; ++index){
        a = (a + data[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    return (b << 16) | a; //Aquí se recorre b 16 bits a la izquierda y después cada bit de b se opera OR con el respectivo bit de a
}

/*Estructura tipo record para incluir la longitud de cadena y los bytes de una información (como un stream de datos, con un puntero al inicio y de ahí sabemos la longitud)*/
typedef struct{
    void *bytes;                //El "void" es para que podamos decir que es un puntero de cualquier tipo de datos
    size_t len;                 //Longitud del contenido
}record;

/*Estos será el tipo de estructura de un elemento de una tabla hash*/
typedef struct {
    record rec;                 //Contenido a guardar en la posición de la tabla
    size_t len;                 //Longitud en bytes del record
    char status;                //Estado del item (ponemos si está libre, si está sucio, etc...)
    char lazy_deleted;          //Bandera para indicar si hubo o no un elemento borrado en esa posición
    char leapt;
    uint32_t key;               //La llave del contenido
} hash_item;                    //Nombre

/*Aquí definimos la estructura de una tabla hash como tal (arreglo de cabezas)*/
typedef struct{
    hash_item *table;              //Dirección del primer elemento en el arreglo de las cabezas
    size_t index_size;          //Índice del tipo de capacidad (arreglo de diferentes tamaños con números impares)
    size_t size;                //Tamaño del arreglo
    size_t occupied_elements;   //Cantidad de elementos ocupados en la tabla
}HTable_OA;

/*Función para hacer una nueva tabla Hash con Open Addressing*/
HTable_OA* newHTableCap_OA(size_t index){
    //Reservamos memoria para la tabla Hash
    HTable_OA *HT = (HTable_OA*)malloc(sizeof(HTable_OA)*1);        //Reserva memoria para la tabla
    if(HT == NULL){                                                //Si HT es NULL, MALLOC no pudo reservar más memoria
        fprintf(stderr, "Cannot allocate memory for table.");
        exit(1);
    }
    //Reservamos memoria para el arreglo de los elementos hash (la tabla misma)
    HT->table = (hash_item*)calloc(HASH_SIZE[index], sizeof(hash_item));
    if(HT->table == NULL){                                          //Si table es NULL, MALLOC no pudo reservar más memoria
        fprintf(stderr, "Cannot allocate memory for table.");
        exit(1);
    }
    //Si llegamos aquí, entonces sí se pudo reservar memoria
    HT->size = HASH_SIZE[index];                              //Indicar el tamaño de la tabla
    HT->index_size = index;                                   //Indicar el índice de tamaño
    //Inicializamos en 0 la cantidad de elementos ocupados en total(apenas es nueva la tabla)
    HT->occupied_elements = 0;
    for(size_t i = 0; i<HT->size; i++){
        HT->table[i].status = NOTVALID;                     //Marcamos los elementos como NOTVALID
        HT->table[i].lazy_deleted = NO;                     //Quitamos bandera de lazy deleted
        HT->table[i].leapt = NO;                            //Quitamos bandera de "elemento saltado"
    }
    return HT;
    }


/*Aquí definimos una función para generar una tabla Hash con arreglos con el primer tamaño disponible*/
HTable_OA* newHTable_OA(){
    return newHTableCap_OA(0);
}

/*Función para liberar el espacio de toda la tabla (elemento por elemento)*/
void freeHTable_OA(HTable_OA *HT){
    //Se libera elemento por elemento
    for(size_t i=0; i<HT->size; i++){
        //Se libera los espacios reservados para el contenido en cada elemento
        //free(HT->table[i].rec.bytes);
    }
    free(HT->table);
    assert(HT->table != NULL);//"Asegúrate de que el arreglo de cabezas no es nulo"
    free(HT);
}

//Funcion para sacar el módulo de una llave
static inline size_t hashFunction(uint32_t key, size_t hashSize){ //static inline hace que el compilador tome el argumento y opere hashFunction sin considerarla como funcion
    return key % hashSize;
}

/*Prototipo para poder usar la función de insertar en la función "Remodel"*/
hash_item* HTinsertRecord_OA(HTable_OA **HT, record *rec, int mode);

/*Función para para expandir o reducir espacio: reserva memoria y reacomoda el contenido de una tabla ya existente*/
HTable_OA* RemodelHTableCap_OA(HTable_OA *PreviousHT, int state, size_t mode){
    //Variable auxiliar para guardar el índice de tamaño de la tabla antigua
    size_t newIndex = PreviousHT->index_size;
    //Ahora aumentamos o disminuimos el tamaño de la tabla según el valor de "state"
    if(state==FULL)
        newIndex+=1;                                       //Incrementamos el valor del cap_type (avanzamos en el arreglo de capacidades)
    if(state==EMPTY)
        newIndex-=1;                                       //Decrementamos el valor del cap_type (retrocedemos en el arreglo de capacidades)

    //Aquí aseguramos que state no sea 0. Si es así, entonces hubo un erro al mandar llamar la función sin necesidad
    //...(DETENTE si la tabla no está ni llena ni vacía)
    assert(state!=0);
    //Creamos una nueva tabla con el nuevo índice
    HTable_OA *HT = newHTableCap_OA(newIndex);
    //Aquí se insertará cada elemento de la tabla antigua a la nueva
    for(size_t i=0; i< ((PreviousHT->size)); i++){
        hash_item aux = PreviousHT->table[i];
            //Si el estado es NOTVALID, es porque ya estaba borrado. Por lo tanto no se vuelve a insertar
            if(aux.status==VALID)
                HTinsertRecord_OA(&HT, &aux.rec, mode);
        }
    //Liberamos el espacio de la tabla antigua
    freeHTable_OA(PreviousHT);
    //Regresamos la nueva tabla (con el contenido incluído)
    return HT;
    }

/*Función para evaluar si la tabla está llena o vacía (relativamente hablando)*/
//NOTA: "operation" indica si se mandó llamar la función para insertar ("UP") o para borrar ("DOWN") elementos
int checkSizeOA(HTable_OA *HT, int operation){
    //Checamos si la cantidad de elementos ocupados es mayor al 50% de capacidad. Si es así, está llena.
    if((HT->occupied_elements>(HT->size/2))&&(operation==UP))
        return FULL;
    //Ahora, se evalúa si la cantidad de elementos ocupados es menor que un cuarto de la capacidad total
    //NOTA: Aquí le sumamos el cuadrado de la variable global "hist" (histéresis)
    size_t aux1 = HT->occupied_elements;
    size_t aux2 = HT->size/10 +(hist*hist);
    if((HT->occupied_elements<(aux2))&&(operation==DOWN)){
        //Por supuesto, si tenemos el menor tamaño posible, no mandamos "empty" para no reducir (ya no se puede)
        if((HT->index_size)>0)
            hist++;                 //Aumentamos el valor de la histéresis en 1 (cada vez que se reduzca la tabla)
            return EMPTY;
    }
    return 0;
}

/*Función para checar los bytes entre dos contenidos y ver si son iguales o no*/
int checkMatchRecord(record *A, record *B);

//************************************FUNCIONES PARA LAS OPERACIONES BÁSICAS*******************************************************************************************
/************************TIPOS DE SONDEO PARA BUSCAR ELEMENTOS***************************************/
/*Función para buscar una llave usando sondeo lineal*/
/*NOTA: index es el resultado de la función hash original*/
hash_item* LPFindKey(HTable_OA **HT, size_t key, record *rec){
    size_t index = hashFunction(key, (*HT)->size);
    //La variable i representa la cantidad de colisiones
    size_t i = 0;
    //Si hubo coincidencia con la llave, se regresa el hash item correspondiente
    if(checkMatchRecord(&((*HT)->table[index].rec), rec)==YES)
        return (&(*HT)->table[index]);
    //Ciclo que recorre toda la tabla hasta dar con un espacio sin lazy deleted (función anticolisiones: f(i)=i)
    while(((*HT)->table[index].lazy_deleted==YES || (*HT)->table[index].leapt==YES)){
        //Si hubo coincidencia con la llave, se regresa el hash item correspondiente
        if(checkMatchRecord(&((*HT)->table[index].rec), rec)==YES)
            return (&(*HT)->table[index]);
        //Se incremente la cantidad de colisiones en 1
        i++;
        index = hashFunction(index + i, (*HT)->size);     //Aquí se aplica h2(i) = (x + f(i)) mod HASH_SIZE
    }
    //Si hubo coincidencia con la llave, se regresa el hash item correspondiente
    if(checkMatchRecord(&((*HT)->table[index].rec), rec)==YES)
        return (&(*HT)->table[index]);

    //Si no se encontró regresa NULL
    return NULL;
}

/*Función para buscar un espacio de tabla disponible con sondeo cuadrático*/
hash_item* QPFindKey(HTable_OA **HT, size_t key, record *rec){
    size_t index = hashFunction(key, (*HT)->size);
    //Si hubo coincidencia con la llave, se regresa el hash item correspondiente
        if(checkMatchRecord(&((*HT)->table[index].rec), rec)==YES)
            return (&(*HT)->table[index]);
    //La variable i representa la cantidad de colisiones
    size_t i = 0;
    //Ciclo que recorre toda la tabla hasta dar con un espacio disponible (función anticolisiones: f(i)=i^2)
     while(((*HT)->table[index].lazy_deleted==YES || (*HT)->table[index].leapt==YES)){
        //Si hubo coincidencia con la llave, se regresa el hash item correspondiente
        if(checkMatchRecord(&((*HT)->table[index].rec), rec)==YES)
            return (&(*HT)->table[index]);
        //Se incremente la cantidad de colisiones en 1
        i++;
        index = hashFunction(index + (i*i), (*HT)->size);     //Aquí se aplica h2(i) = (x + f(i)) mod HASH_SIZE
    }
    //Si hubo coincidencia con la llave, se regresa el hash item correspondiente
        if(checkMatchRecord(&((*HT)->table[index].rec), rec)==YES)
            return (&(*HT)->table[index]);
    return NULL;
}

/*Función para buscar un espacio de tabla disponible con double hashing*/
hash_item* DHFindKey(HTable_OA **HT, size_t key, record *rec){
    size_t index = hashFunction(key, (*HT)->size);
    //La variable i representa la cantidad de colisiones
    size_t i = 0;
    //Si hubo coincidencia con la llave, se regresa el hash item correspondiente
    if(checkMatchRecord(&((*HT)->table[index].rec), rec)==YES)
        return (&(*HT)->table[index]);
    //Ciclo que recorre toda la tabla hasta dar con un espacio disponible (función anticolisiones: f(i)= R - i mod R, ...
    //... siendo R un número primo menor a HASH_SIZE)
    //Se define primeramente R (véase comentario anterior) con el número primo previo al de HASH_SIZE según el
    //arreglo de capacidades posibles. Sólo se hace la excepción para cuando es la primera capacidad posible
    size_t R;
    //Si el tamaño de la tabla es el mínimo, establecemos R=3 (primo menor al mínimo tamaño, el cual es 5)
    if((*HT)->size <= 5){
        R = 3;
    }
    else
    //Se establece como R el primo menor anterior en el arreglo de capacidades
        R = HASH_SIZE[(*HT)->index_size - 1];
    size_t Hash2;
     while(((*HT)->table[index].lazy_deleted==YES || (*HT)->table[index].leapt==YES)){
        //Si hubo coincidencia con la llave, se regresa el hash item correspondiente
        if(checkMatchRecord(&((*HT)->table[index].rec), rec)==YES)
            return (&(*HT)->table[index]);
        //Se incremente la cantidad de colisiones en 1
        i++;
        //Se realiza aquí el double hashing
        Hash2 = R - hashFunction(key, R);
        index = hashFunction((index + i*Hash2),(*HT)->size);     //Aquí se aplica h2(i) = (x + f(i)) mod HASH_SIZE

    }
    //Si hubo coincidencia con la llave, se regresa el hash item correspondiente
        if(checkMatchRecord(&((*HT)->table[index].rec), rec)==YES)
            return (&(*HT)->table[index]);
    return NULL;
}

/***************************************************************************************/
/*Función para encontrar una llave en una tabla Hash*/
hash_item* HTfindkey_OA(HTable_OA **HT, uint32_t key, size_t mode, record *rec){
    switch (mode)
    {
    //Se manda llamar la función para buscar una llave según see el modo operado
    case LP:
        return LPFindKey(HT, key, rec);
        break;
    case QP:
        return QPFindKey(HT, key, rec);
        break;
    case DH:
        return DHFindKey(HT, key, rec);
        break;
    default:
        break;
    }
    return NULL;
}

/*Función para checar los bytes entre dos contenidos y ver si son iguales o no*/
int checkMatchRecord(record *A, record *B){
    //Si la longitud de A y B son diferentes, de antemano ya sabemos que no son iguales
    if(A->len != B->len)
        return NO;
    unsigned char *pA = (unsigned char*)A->bytes;            // Este "(unsigned char*)" es lo que conocemos como un untpype cast
    unsigned char *pB = (unsigned char*)B->bytes;
    //Si uno de los bytes es diferente entre sí de A y B, entonces no son iguales
    for(size_t i=0; i<A->len; i++){
        if(pA[i] != pB[i])
            return NO;
    }
    //Si A y B pasaron las pruebas anteriores, entonces sí son iguales
    return YES;
}

/*Función para checar los bytes entre dos contenidos y ver si son iguales o no*/
int checkMatchRecord2(record *A, record *B){
    //Si la longitud de A y B son diferentes, de antemano ya sabemos que no son iguales
    if(A->len != B->len)
	return NO;
    unsigned char *pA = (unsigned char*)A->bytes;            // Este "(unsigned char*)" es lo que conocemos como un untpype cast
    unsigned char *pB = (unsigned char*)B->bytes;
    //Si uno de los bytes es diferente entre sí de A y B, entonces no son iguales
    for(size_t i=0; i<B->len; i++){
        if(pA[i] != pB[i]){
            ++aux;
	    return NO;}
    }
    //Si A y B pasaron las pruebas anteriores, entonces sí son iguales
    return YES;
}

/*Función para encontrar un record en una tabla Hash*/
hash_item* HTfindRecord_OA(HTable_OA **HT, record *rec, size_t mode){
    //Se calcula la llave de acuerdo al contenido
    uint32_t key = adler32((unsigned char*)rec->bytes, rec->len);               //Encuentro la llave asociada a record (una cadena de longitud "len")
    //Se manda llamar la función de encontrar llave
    hash_item *item = HTfindkey_OA(HT, key, mode, rec);
    if(item == NULL)
        return NULL;
    //Si se encontró la llave, se verifica si hay coincidencia en el contenido
    if(checkMatchRecord(rec, &(item->rec))==YES)
        return item;
    return NULL;
}

/*Función para encontrar un record en una tabla Hash*/
hash_item* HTfindRecord_OA2(HTable_OA **HT, record *rec, size_t mode){
    //Se calcula la llave de acuerdo al contenido
    uint32_t key = adler32((unsigned char*)rec->bytes, rec->len);               //Encuentro la llave asociada a record (una cadena de longitud "len")
    //Se manda llamar la función de encontrar llave
    hash_item *item = HTfindkey_OA(HT, key, mode, rec);
    if(item == NULL)
        return NULL;
    //Si se encontró la llave, se verifica si hay coincidencia en el contenido
    if(checkMatchRecord2(rec, &(item->rec))==YES)
        return item;
    return NULL;
}

/************************TIPOS DE SONDEO PARA INSERTAR ELEMENTOS***************************************/
/*Función para buscar un espacio de tabla disponible con sondeo lineal*/
/*NOTA: index es el resultado de la función hash original*/
size_t LinealProbing(HTable_OA **HT, size_t index){
    //La variable i representa la cantidad de colisiones
    size_t i = 0;
    //Ciclo que recorre toda la tabla hasta dar con un espacio disponible (función anticolisiones: f(i)=i)
    while(((*HT)->table[index].status==VALID)){
        //Se incremente la cantidad de colisiones en 1
        i++;
        //Se marca el elemento actual como saltado
        (*HT)->table[index].leapt=YES;
        index = hashFunction(index + i, (*HT)->size);     //Aquí se aplica h2(i) = (x + f(i)) mod HASH_SIZE
    }
    return index;
}

/*Función para buscar un espacio de tabla disponible con sondeo cuadrático*/
size_t QuadraticProbing(HTable_OA **HT, size_t index){
    //La variable i representa la cantidad de colisiones
    size_t i = 0;
    //Ciclo que recorre toda la tabla hasta dar con un espacio disponible (función anticolisiones: f(i)=i^2)
    while(((*HT)->table[index].status==VALID)){
        //Se incremente la cantidad de colisiones en 1
        i++;
        //Se marca el elemento actual como saltado
        (*HT)->table[index].leapt=YES;
        index = hashFunction(index + (i*i), (*HT)->size);     //Aquí se aplica h2(i) = (x + f(i)) mod HASH_SIZE
    }
    return index;
}

/*Función para buscar un espacio de tabla disponible con double hashing*/
size_t DoubleHashing(HTable_OA **HT, size_t key){
    size_t index = hashFunction(key, (*HT)->size);
    //La variable i representa la cantidad de colisiones
    size_t i = 0;
    //Ciclo que recorre toda la tabla hasta dar con un espacio disponible (función anticolisiones: f(i)= R - i mod R, ...
    //... siendo R un número primo menor a HASH_SIZE)
    //Se define primeramente R (véase comentario anterior) con el número primo previo al de HASH_SIZE según el
    //arreglo de capacidades posibles. Sólo se hace la excepción para cuando es la primera capacidad posible
    //size_t R = HASH_SIZE[(*HT)->index_size - 1];
    size_t R;
    if((*HT)->size <= 5){
        R = 3;
    }
    else
        R = HASH_SIZE[(*HT)->index_size - 1];
    size_t Hash2;
    while(((*HT)->table[index].status==VALID)){
        //Se incremente la cantidad de colisiones en 1
        i++;
        //Se marca el elemento actual como saltado
        (*HT)->table[index].leapt=YES;
        Hash2 = R - hashFunction(key, R);
        index = hashFunction((index + i*Hash2), (*HT)->size);     //Aquí se aplica h2(i) = (x + f(i)) mod HASH_SIZE
    }
    return index;
}

/*************************************************************************************************/

/*Función para insertar un elemento en una tabla hash*/
/*NOTA: la variable local "mode" es para indicar qué tipo de sonde se empleará*/
hash_item* HTinsertRecord_OA(HTable_OA **HT, record *rec, int mode){
    //Primeramente vamos a ver si la tabla tiene un tamaño grande. Si es así, la expandemos
    if(checkSizeOA(*HT, UP)==FULL){
        (*HT)=RemodelHTableCap_OA(*HT, FULL, mode);
    }
    //Se calcula la llave
    uint32_t key = adler32(rec->bytes, rec->len);
    //Usando la función para encontrar una llave, se evalúa si lo que regresa es nulo o no (si no lo es, quiere decir que ya estaba el contenido...
    //... en la tabla)
    if(HTfindRecord_OA(HT, rec, mode) != NULL)
        return NULL;
    
    //Si la ejecución llega hasta aquí, el contenido no estaba presente.
    //Se realiza la función hash original
    size_t index = hashFunction(key, (*HT)->size);
    //A continuación realizamos la búsqueda de un espacio disponible según el tipo de sondeo elegido en MAIN
    switch (mode)
    {
    case LP:
        index = LinealProbing(HT, index);
        break;
    case QP:
        index = QuadraticProbing(HT, index);
        break;
    case DH:
        index = DoubleHashing(HT, key);
        break;
    default:
        break;
    }
    //Insertamos el record en el lugar encontrado
    (*HT)->table[index].key = key;
    (*HT)->table[index].status = VALID;
    (*HT)->table[index].rec.bytes = malloc(sizeof(rec->len));
    (*HT)->table[index].rec.len = rec->len;
    if((*HT)->table[index].rec.bytes == NULL){
        fprintf(stderr, "Cannot allocate memory for element!\n");
        return NULL;
    }
    //Se copia el contenido
    strcpy((*HT)->table[index].rec.bytes, rec->bytes);
    (*HT)->occupied_elements++;
    return (*HT)->table;
}

//Función para borrar un record en una tabla hash
void HTdeleteRecordOA(HTable_OA **HT, record *rec, size_t mode){
    //Se verifica si no exisitía antes el record en la tabla
    hash_item* item = HTfindRecord_OA2(HT, rec, mode);
    if(item == NULL)
        return;
    item ->status = NOTVALID;
    item ->lazy_deleted = YES;
    //Finalmente vamos a ver si la tabla tiene muchos elementos sin ocupar. Si es así, la reducimos
    if(checkSizeOA(*HT, DOWN)==EMPTY){
        if((*HT)->index_size>0){
            (*HT)=RemodelHTableCap_OA(*HT, EMPTY, mode);
        }
    }
    //Reducimos en uno el número de elementos ocupados
    if((*HT)->occupied_elements>0)
    	(*HT)->occupied_elements--;
    return;
}

/*Función para imprimir el contenido de un elemento de la tabla caracter por caracter*/
void HTprintItem_OA(hash_item *item){
    //Si tiene el estado "NOTVALID", no imprimir
    if(item->status==NOTVALID)
        return;
    char *str = (char*)item->rec.bytes;
    for(int i = 0; i<item->rec.len; i++){
        printf("%c", str[i]);
        /*if(str[i]==NULL){
            break;
        }*/
    }
    uint32_t key = item->key;
    printf("[%u] ", key);
}

/*Función para imprimir una tabla hash con open addressing*/
void HTprint_OA(HTable_OA *HT){
    for(size_t i=0; i<HT->size; i++){
        printf("%ld ", i);
        HTprintItem_OA(&(HT->table[i]));
    printf("\n");
    }
}


//************************************INT MAIN********************************************************************************************
int main(int argc, char **argv){
    HTable_OA *HT = newHTable_OA();
    size_t mode;
    //Aquí se elige manualmente el tipo de sondeo a emplear (LP = Lineal Proubing, QP = Quadratic Proubing y DH = Double Hashing)
    if(argc == 1){
        printf("Bienvenid@. Eliga la estrategia (1 = Lineal Proubing, 2 = Quadratic Proubing y 3 = Double Hashing): ");
        scanf("%ld", &mode);
    }
    else{
        mode = atoi(argv[1]);
    }
    if(mode!=1 && mode!=2 && mode!=3)
        return 0;
    record rec;
    char buffer[100];
    //int cont = 0;
    while(fgets(buffer, 100, stdin) != NULL){
        char command[20] = " ";
        char number[30] = " ";
        sscanf(buffer, "%s %s", command, &number);     //Recuerda usar el espacio para separar
        rec.bytes = number;
        rec.len = strlen(number);
        if(strcmp("insert", command)==0){               //insertar
            HTinsertRecord_OA(&HT, &rec, mode);      
            continue;
        }
        if(strcmp("delete", command)==0){               //borrar
            HTdeleteRecordOA(&HT, &rec, mode);
            continue;
        }
        if(strcmp("print", command)==0){                //imprimir
            HTprint_OA(HT);
            continue;
            break;
        }
        if(strcmp("stop", command)==0){                 //Parar (crea un ciclo infinito para medir memoria en servidor)
            size_t i = 0;
            while(i<1){
                continue;
            }
            continue;
        }
	 if(strcmp("count", command)==0){              //Imprimir no. de elementos en la tabla
                    printf("Elementos ocupados: %ld\n", HT->occupied_elements);
            continue;}
        if(strcmp("exit", command)==0)                  //salir
            break;
    }
    freeHTable_OA(HT);
    printf("Gracias!\n"); 
    printf("Contador auxiliar: %d\n", aux);	
    return 0;
}

//Proof


