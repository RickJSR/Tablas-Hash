//Esta es una biblioteca con una estructura que implementa una tabla hash como Separate Chaining

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
//#include <math.h>
#include <time.h>

//NOTA 1: El tipo size_t facilita el trabajo con variables que solo almacenan valores enteros positivos (size_t es el tamaño máximo que
//...maneja la computadora)
//NOTA 2: A "uint32_t" is guaranteed to take up exactly 32-bits of space, whereas other types are implementation-specific
//NOTA 3: stderr es, como su nombre indica, la salida estandar de errores. este es util cuando por ejemplo, rediriges la salida de tu
//...programa a un archivo. El error saldra en la terminal en vez de en el archivo.
//NOTA 4: "assert()" evalúa si lo que está dentro de los paréntesis es non-zero (TRUE) o zero (FALSE). Si es cero, manda mensaje de
//..error por stderr y termina la ejecución del programa

//Definimos macros (cuando el PC compile, YES lo traduce a 1 y NO a 0... no son variables globales)
#define YES 1
#define NO 0
#define VALID 1
#define NOTVALID 0
#define DELETED NOTVALID
#define MAX_64 2147483647
#define FULL 2
#define EMPTY 1
#define UP 1
#define DOWN 0
#define LL 1
#define AR 0

//En este arreglo se contienen los números primos menores a cada potencia de 2 (hasta 2^16)
const uint32_t HASH_SIZE[] = {5, 23, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749, 65521, 131071, 262139, 524287, 1048573, 2097143, 4194301, 8388593, 16777213, 33554393, 67108859, 134217689, 268435399, 536870909, 1073741789, 2147483647, 4294967291};

//Constante de ADLER
const uint32_t MOD_ADLER = 65521;

//Variable Global para la histéresis (tolerancia para rehash down en casos donde el usuario inserte y borre alternadamente)
int hist = 0;

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
    char status;                //Estado del item (ponemos si borrado o no)
    uint32_t key;               //La llave del contenido
} hash_item;                    //Nombre

/*Estructura para cada lista ligada en cada posición de la tabla hash*/
struct LinkedList_Hash{         //Definición de la estructura
    hash_item elem;             //Contenido del nodo de la lista
    struct LinkedList_Hash *next;    //Link (dirección) del siguiente nodo
};

typedef struct LinkedList_Hash LLHash; //Definimos un tipo de datos "LLHash" el cual es una estructura LinkedList (recuerda el ejemplo de analogía de int con misInts) -> Cada LLHash es un elemento de una lista ligada

/*Aquí definiremos la cabeza de un elemento de la tabla hash*/
typedef struct{
    size_t n;                  //No. de elementos en la lista ligada
    LLHash *next;              //Dirección del primer nodo de lista ligada conectada a la cabeza
}LLHead;                       //Nombre


/*Aquí definimos la estructura de una tabla hash como tal (arreglo de cabezas LLHead)*/
typedef struct{
    LLHead *table;              //Dirección del primer elemento en el arreglo de las cabezas
    size_t index_size;          //Índice del tipo de capacidad (arreglo de diferentes tamaños con números impares)
    size_t size;                //Tamaño del arreglo
    size_t occupied_elements;   //Cantidad de elementos ocupados en la tabla
}HTable_SC;

/*Realiza una nueva tabla definiendo su tamaño, su índice y se realiza un malloc para apartar memoria. Regresa la dirección de donde empieza la tabla*/
HTable_SC* newHTableCap_SC(size_t index){
    HTable_SC *HT = (HTable_SC*)malloc(sizeof(HTable_SC)*1);    //Reserva memoria para una tabla
    if(HT == NULL){                                             //Si HT es NULL, MALLOC no pudo reservar más memoria
        fprintf(stderr, "Cannot allocate memory for table.");
        exit(1);
    }
    //Si llegamos aquí, entonces sí se pudo reservar memoria
    HT->size = HASH_SIZE[index];
    HT->index_size = index;                                   //Marcar (con llamada a 0) en el primer elemento
    HT->table = (LLHead*)calloc(HT->size, sizeof(LLHead));    //CALLOC reserva memoria para todo un arreglo inicializando en 0 cada uno de sus elementos
    //La tabla ya está inicializada porque usamos CALLOC; son dos vaiables a inicializar: n y el puntero de next (Deben ser cero y NULL)
    if(HT->table == NULL){
        fprintf(stderr, "Cannot allocate memory for table.");
        exit(1);
    }
    //Inicializamos en 0 la cantidad de elementos ocupados (apenas es nueva la tabla)
    HT->occupied_elements = 0;
    return HT;
}

/*Aquí definimos una función que genera una dirección de donde iniciará una tabla nueva*/
HTable_SC* newHTable_SC(){
    return newHTableCap_SC(0);
}

/*Función que libera todo elemento de la lista ligada de una cabeza*/
void freeLLHashItem(LLHash *item){
    //Parte que termina la recursividad: si el próximo elemento de la lista (de la cabeza) es nulo, regresa
    if(item == NULL){
        return;
    }
    //Recursividad: manda a llamar la función de todos los elementos hasta llegar a nulo
    freeLLHashItem(item->next);
    //Cuando la recursividad acaba, aquí sigue la ejecución
    assert(item != NULL);       //Asegurar que en efecto, el item NO es null (sino para dejar de ejecutar)
    free(item->elem.rec.bytes); //Liberar espacio del contenido
    free(item);                 //Liberar espacio del elemento actual
    
}


/*Función para liberar el espacio de toda la tabla (cabeza por cabeza)*/
void freeHTable_SC(HTable_SC *HT){
    //Se libera cabeza por cabeza
    for(size_t i=0; i<HT->size; i++){
        //Llamada a función que libera espacios de cabeza
        freeLLHashItem(HT->table[i].next);
    }
    assert(HT->table != NULL);//"Asegúrate de que el arreglo de cabezas no es nulo"
    free(HT->table);
    assert(HT != NULL);//"Asegúrate de que la tabla hash no es nula"
    free(HT);
}


//Funcion para sacar el módulo de una llave
static inline size_t hashFunction(uint32_t key, size_t hashSize){ //static inline hace que el compilador tome el argumento y opere hashFunction sin considerarla como funcion
    return key % hashSize;
}

/*Prototipo de la función para insertar (y así poder usarla en la función de expandir la tabla)*/
hash_item* HTinsertRecord_SC(HTable_SC **HT, record *rec);

/*Función para para expandir o reducir espacio: reserva memoria y reacomoda el contenido de una tabla ya existente*/
HTable_SC* RemodelHTableCap_SC(HTable_SC *PreviousHT, int state){
    //Variable auxiliar para guardar el índice de tamaño de la tabla antigua
    size_t newIndex = PreviousHT->index_size;
    //Ahora aumentamos o disminuimos el tamaño de la tabla según el valor de "state"
    if(state==FULL)
        newIndex+=1;                                       //Incrementamos el valor del cap_type (avanzamos en el arreglo de capacidades)
    if(state==EMPTY)
        newIndex-=1;                                       //Decrementamos el valor del cap_type (retrocedemos en el arreglo de capacidades)

    //Aquí aseguramos que state no sea 0. Si es así, entonces hubo un erro al mandar llamar la función sin necesidad
    //...(la tabla no está ni llena ni vacía)
    assert(state!=0);
    //Creamos una nueva tabla con el nuevo índice
    HTable_SC *HT = newHTableCap_SC(newIndex);
    //Aquí se insertará cada elemento de la tabla antigua a la nueva
    for(size_t i=0; i< ((PreviousHT->size)); i++){
        LLHash *aux = PreviousHT->table[i].next;
        while(aux!=NULL){
            //Si el estado es NOTVALID, es porque ya estaba borrado. Por lo tanto no se vuelve a insertar
            if(aux->elem.status==VALID)
                HTinsertRecord_SC(&HT, &aux->elem.rec);
            aux = aux->next;
        }
    }
    //Liberamos el espacio de la tabla antigua
    freeHTable_SC(PreviousHT);
    //Regresamos la nueva tabla (con el contenido incluído)
    return HT;
}

/*Función para evaluar si la tabla está vacía o llena*/
//NOTA: "operation" indica si se mandó llamar la función para insertar ("UP") o para borrar ("DOWN") elementos
int checkSize(HTable_SC *HT, int operation){
    //Se suma la cantidad de elementos en total de la tabla (se suman los elementos por cabeza en el siguiente ciclo)
    size_t sum = 0;
    for(size_t i=0; i< ((HT->size)); i++){
        sum += HT->table[i].n;
    }

    //Si la suma de elementos es mayor que el cuadrado del tamaño de la tabla, indicamos que está "llena"
    if((sum>=((HT->size)*(HT->size)))&&operation==UP){
        return FULL;
    }
    //Ahora, se evalúa si la cantidad de elementos ocupados es menor que el cuarto de la tabla. Si es así,
    //...indicamos que está "vacía".
    //NOTA: Aquí le sumamos el cuadrado de la variable global "hist" (histéresis)
    size_t NextSize;
    if((HT->occupied_elements<(sum/4+(hist*hist)))&&(operation==DOWN)){
        //Por supuesto, si tenemos el menor tamaño posible, no mandamos "empty" para no reducir (ya no se puede)
        if((HT->index_size)>0){
           size_t NextSize = HASH_SIZE[HT->index_size - 1];
            if(HT->occupied_elements < (NextSize*NextSize)){
                hist++;                 //Aumentamos el valor de la histéresis en 1 (cada vez que se reduzca la tabla)
                return EMPTY;}
        }
    }
    return 0;
}

//************************************FUNCIONES PARA LAS OPERACIONES BÁSICASS********************************************************************************************
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

/*Función para encontrar elementos según su llave. Regresa el puntero de un Hash Item (búsqueda robusta)*/
hash_item *HTfindkey_SC(HTable_SC **HT, uint32_t key){
    size_t index = hashFunction(key, (*HT)->size);
    LLHash *current = (*HT)->table[index].next;         //Current es un LLHash (un elemento de la lista ligada de una cabeza)
    while(current != NULL){
        if(current->elem.key == key){                   //Buscar a lo largo de una lista ligada el elemento asociado a la llave de interés
            return &(current->elem);                    //Regresa NO la dirección, sino el elemento
        }
        current = current->next;                        //Siguiente nodo de la lista ligada
    }
    return NULL;                                        //Si la ejecución llega hasta aquí, no se encontró nada con la llave
}

/*Función para encontrar el contenido (record) de un elemento en una tabla Hash según una llave (búsqueda preliminar)*/
hash_item* HTfindRecord_SC(HTable_SC **HT, record *rec){               //El const char es para que la función no altere la dirección de record
    uint32_t key = adler32((unsigned char*)rec->bytes, rec->len);               //Encuentro la llave asociada a record (una cadena de longitud "len")
    hash_item *item = HTfindkey_SC(HT, key);            //Regrésame la dirección del item asociada a la llave
    //Si el item es nulo, simplemente no hay contenido
    if(item == NULL)
        return NULL;
    //Si se encuentra el contenido correspondiente, entonces regresa la dirección del contenido
    if(checkMatchRecord(rec, &(item->rec))==YES)
        return item;
    //Si no es nulo, entonces vamos a buscar
    //Esto evalúa si hay match entre dos records a una misma llave
    //Si encuentro un record diferente con la misma llave, hay que buscar con mayor resolución (búsqueda más exhaustiva)
    size_t index = hashFunction(key, (*HT)->size);
    LLHash *current = (*HT)->table[index].next;         //Current es un LLHash (un elemento de la lista ligada de una cabeza)
    while(current != NULL){
        //En el siguiente IF se evalúa la primera condición por mejorar el rendimiento: es más fácil evaluar, y si truena, ya no es necesario hacer la siguiente evaluación
        if(current->elem.key == key && checkMatchRecord(rec, &(current->elem.rec))==YES){                   //Buscar a lo largo de una lista ligada el elemento asociado a la llave de interés
            if(current->elem.status == VALID)
            return &(current->elem);
        }
        current = current->next;                        //Siguiente nodo de la lista ligada
    }
    return NULL;                                        //Si la ejecución llega hasta aquí, no se encontró nada con la llave
    }



/*Función para introducir un contenido (Record) en la tabla. Regresará la dirección de dónde se insertó el nuevo contenido*/
hash_item* HTinsertRecord_SC(HTable_SC **HT, record *rec){
    //Primeramente vamos a ver si la tabla tiene un tamaño grande. Si es así, la expandemos
    if(checkSize(*HT, UP)==FULL){
        (*HT)=RemodelHTableCap_SC(*HT, checkSize(*HT, UP));
        //printf("Cambiamos el tamaño");
    }
    //Vemos si el contenido ya está
    hash_item *item = HTfindRecord_SC(HT, rec);
    //Si es diferente de nulo, significa que ya estaba
    if(item != NULL){
        return item;
    }

    //Si ese item es NULO, entonces no estaba el dato guardado previamente. Calculemos pues la llave
    uint32_t key = adler32((unsigned char*)rec->bytes, rec->len);
    //Sacamos el módulo de la llave
    size_t index = hashFunction(key, (*HT)->size);
    //Si la ejecución llega hasta este punto, tenemos la garantía de que no había ese dato ya existente previamente
    LLHash *list = (*HT)->table[index].next;            //Aquí declaramos un elemento de lista (conectada a la cabeza correspondiente)
    //Si el primer elemento de la lista es nulo (no habíamos insertado nada ahí), se reserva memoria y se inserta el elemento ahí
    if(list == NULL){
        list = (LLHash*)malloc(sizeof(LLHash)*1);
        //Si no se logra, seguirá siendo NULL
        if(list == NULL){
            fprintf(stderr, "Cannot allocate memory for element!\n");
            return NULL;
        }

        //Inserta el elemento aquí
        list->next = NULL;
        list->elem.key = key;
        list->elem.status = VALID;
        list->elem.rec.bytes = malloc((rec->len));        // OJO: Aquí apenas se reserva la memoria necesaria para copiar el contenido
        //Colocamos lo siguiente en caso de que no se haya podido reservar memoria
        if(list->elem.rec.bytes == NULL){
            fprintf(stderr, "Cannot allocate memory for element!\n");
            return NULL;
        }
        list->elem.rec.len = rec->len;
        //La siguiente función es para copiar directamente el contenido a un destino con esta forma: (destino, remitente, tamaño)
        //OJO: En el destinatario debe caber lo de la fuente.
        memcpy(list->elem.rec.bytes, rec->bytes, rec->len);
        //Ahora sí, ligamos este nuevo "list" a la cabeza donde queremos insertar el elemento
        (*HT)->table[index].next = list;
        //Aumentamos el contador de elementos en uno (conteo de elementos conectados a la cabeza)
        (*HT)->table[index].n++;
        //Incrementamos en 1 el contador de elementos ocupados en la tabla
        (*HT)->occupied_elements++;
        return &(list->elem);
    }
    //Si la ejecución llega a esta parte, la lista ligada ya tenía su primer elemento ocupado
    LLHash *current = list;
    while(current != NULL){
        //Este es para ingresar un elemento donde había un NOT VALID, no al final de la lista
        if(current->elem.status == NOTVALID){
            current->elem.key = key;
            current->elem.status = VALID;
            //Ahora, asegura que los bytes a reservar sí existen (y no es un puntero nulo)
            assert(current->elem.rec.bytes != NULL);
            //Si en este elemento "desocupado" que encontramos tiene el suficiente espacio para almacenar el nuevo dato, lo colocamos ahí
            if(rec->len > current->elem.rec.len){
                //Aquí vamos a usar realloc: realloc("puntero del bloque de memoria reservado previamente", "espacio a recolocar ahí")
                current->elem.rec.bytes = realloc(current->elem.rec.bytes, rec->len);
                if(current->elem.rec.bytes == NULL){
                    fprintf(stderr, "Cannot allocate memory for element!\n");
                   return NULL;
                }
            //Aquí se indica el nuevo tamaño ocupado en el elemento
            current->elem.rec.len = rec->len;
            //Aquí se almacena el record
            memcpy(current->elem.rec.bytes, rec->bytes, rec->len);
            //Aumentamos el contador de elementos en uno (conteo de elementos conecados a la cabeza)
            (*HT)->table[index].n++;

            //Aumentamos el contador de elementos ocupados en uno
            (*HT)->occupied_elements++;
            //printf("Se inserto2");
            return &(current->elem);
            }
        }
        //Este es el caso para cuando llegamos al último elemento (los demás ya estaban ocupados)
        if(current->next == NULL) break;
        //Si aun no llegamos al último elemento, continuamos con el que sigue
        current = current->next;
    }
    //Si llegamos hasta el último elemento y no se pudo colocar el contenido en un espacio reservado, lo ponemos al final de la lista ligada
    //...(creamos un nuevo elemento desde cero)

    current->next = (LLHash*)malloc(sizeof(LLHash)*1);
    if(current->next == NULL){
        fprintf(stderr, "Cannot allocate memory for element!\n");
        return NULL;
    }
    current->next->elem.key = key;
    current->next->elem.status = VALID;
    current->next->elem.rec.bytes = malloc(rec->len);
    if(current->next->elem.rec.bytes == NULL){
        fprintf(stderr, "Cannot allocate memory for element!\n");
        return NULL;
    }
    current->next->elem.rec.len = rec->len;
    memcpy(current->next->elem.rec.bytes, rec->bytes, rec->len);
    //strcpy(current->next->elem.rec.bytes, rec->bytes);
    current->next->next = NULL;
    //Aumentamos el contador de elementos en uno (conteo de elementos conecados a la cabeza)
    (*HT)->table[index].n++;
    //Aumentamos el contador de elementos ocupados en uno
    (*HT)->occupied_elements++;
    return &(current->next->elem);
}

/*Función para borrar un elemento de la tabla*/
void HTdeleteRecord(HTable_SC **HT, record *rec){
    //Primero se busca el elemento (para ver si ya estaba dentro)...
    hash_item *item = HTfindRecord_SC(HT, rec);
    //Si la función anterior no se encontró, se regresará un NULL. Si es así, simplemente termina la función (nada por borrar)
    if(item == NULL)
        return;
    //Si en efecto ya estaba el elemento presente en la tabla, simplemente le asignamos "NOTVALID"
    item->status = NOTVALID;
    //Decrementamos el contador de elementos ocupados en uno
    (*HT)->occupied_elements--;
    //Finalmente vamos a ver si la tabla tiene muchos elementos sin ocupar. Si es así, la reducimos
    if(checkSize(*HT, DOWN)==EMPTY){
        if((*HT)->index_size>0)
        (*HT)=RemodelHTableCap_SC(*HT, checkSize(*HT, DOWN));
        //printf("Cambiamos el tamaño");
    }
    return;
}

/*Función para imprimir el contenido de un elemento de la tabla caracter por caracter*/
void HTprintItem_SC(hash_item *item){
    //Si tiene el estado "NOTVALID", no imprimir
    if(item->status==NOTVALID)
        return;
    char *str = (char*)item->rec.bytes;
    for(int i = 0; i<item->rec.len; i++){

        printf("%c", str[i]);
    }
    uint32_t key = item->key;
    printf("[%u] ", key);
}

/*Función para imprimir una tabla*/
void HTprint_SC(HTable_SC *HT){
    for(size_t i=0; i<HT->size; i++){
        printf("%ld ", i);
        LLHash *current = HT->table[i].next;
        while(current != NULL){
            HTprintItem_SC(&(current->elem));
            current = current->next;
        }
    printf("\n");
    }
}
/*..................................................ARRAYS..........................................................................*/
/*Aquí definiremos la cabeza de un elemento de la tabla hash*/
typedef struct{
    size_t len;                   //No. de elementos en el arreglo
    hash_item *elem;              //Dirección del primer elemento del arreglo
}AHead;                           //Nombre


/*Aquí definimos la estructura de una tabla hash como tal (arreglo de cabezas LLHead)*/
typedef struct{
    AHead *table;              //Dirección del primer elemento en el arreglo de las cabezas
    size_t index_size;          //Índice del tipo de capacidad (arreglo de diferentes tamaños con números impares)
    size_t size;                //Tamaño del arreglo
    size_t occupied_elements;   //Cantidad de elementos ocupados en la tabla
}HTable_SCA;

/*Función para hacer una nueva tabla Hash con arreglos*/
HTable_SCA* newHTableCap_SCA(size_t index){
    //Reservamos memoria para la tabla Hash
    HTable_SCA *HT = (HTable_SCA*)malloc(sizeof(HTable_SCA)*1);    //Reserva memoria para una tabla
    if(HT == NULL){                                                //Si HT es NULL, MALLOC no pudo reservar más memoria
        fprintf(stderr, "Cannot allocate memory for table.");
        exit(1);
    }
    //Reservamos memoria para el arreglo de cabezas
    HT->table = (AHead*)malloc(sizeof(AHead)*HASH_SIZE[index]);     //Reserva memoria para un arreglo
    if(HT->table == NULL){                                          //Si table es NULL, MALLOC no pudo reservar más memoria
        fprintf(stderr, "Cannot allocate memory for table.");
        exit(1);
    }
    //Inicializamos todas las cabezas con arreglos (tamaño inicial = 1)
    for(size_t i=0; i<HASH_SIZE[index]; i++){
        HT->table[i].elem = calloc(1,sizeof(hash_item)*1);      //Aquí se crean los "subarreglos"
        HT->table[i].len = 1;
    }
    //Si llegamos aquí, entonces sí se pudo reservar memoria
    HT->size = HASH_SIZE[index];
    HT->index_size = index;                                   //Indicar el índice de tamaño
    //Inicializamos en 0 la cantidad de elementos ocupados en total(apenas es nueva la tabla)
    HT->occupied_elements = 0;
    return HT;
    }


/*Aquí definimos una función para generar una tabla Hash con arreglos con el primer tamaño disponible*/
HTable_SCA* newHTable_SCA(){
    return newHTableCap_SCA(0);
}

/*Función que libera todo contenido en el arreglo de una cabeza*/
void freeLLHashItemSCA(hash_item *item, size_t len){
    //Parte que termina la recursividad: si el próximo elemento de la lista (de la cabeza) es nulo, regresa
    for(size_t i=0; i<len; i++){
        if(item[i].rec.bytes == NULL){
            return;
        }
    }
    free(item);                   //Se libera el elemento del arreglo
    return;
}


/*Función para liberar el espacio de toda la tabla con arreglos (cabeza por cabeza)*/
void freeHTable_SCA(HTable_SCA *HT){
    //Se libera cabeza por cabeza
    for(size_t i=0; i<HT->size; i++){
        freeLLHashItemSCA(HT->table[i].elem, HT->table[i].len);
    }
    assert(HT->table != NULL);//"Asegúrate de que el arreglo de cabezas no es nulo"
    free(HT->table);
    assert(HT != NULL);//"Asegúrate de que la tabla hash no es nula"
    free(HT);
}

/*Prototipo para poder usar la función de insertar en la función "Remodel"*/
void HTinsertRecord_SCA(HTable_SCA **HT, record *rec);

/*Función para para expandir espacio: reserva memoria y reacomoda el contenido de una tabla ya existente*/
HTable_SCA* RemodelHTableCap_SCA(HTable_SCA *PreviousHT, int state){
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
    HTable_SCA *HT = newHTableCap_SCA(newIndex);
    //Aquí se insertará cada elemento de la tabla antigua a la nueva
    for(size_t i=0; i< ((PreviousHT->size)); i++){
        for(size_t j=0; j< (PreviousHT->table[i].len); j++){
            hash_item aux = PreviousHT->table[i].elem[j];
            //Si el estado es NOTVALID, es porque ya estaba borrado. Por lo tanto no se vuelve a insertar
            if(aux.status==VALID)
                HTinsertRecord_SCA(&HT, &aux.rec);
        }
    }
    //Liberamos el espacio de la tabla antigua
    freeHTable_SCA(PreviousHT);
    //Regresamos la nueva tabla (con el contenido incluído)
    return HT;
}

/*Función para evaluar si la tabla está llena o vacía (relativamente hablando)*/
//NOTA: "operation" indica si se mandó llamar la función para insertar ("UP") o para borrar ("DOWN") elementos
int checkSizeSCA(HTable_SCA *HT, int operation){
    //Checamos si la cantidad de elementos ocupados es mayor que H_SIZE^2. Si es así, marcamos a la tabla como llena
    if((HT->occupied_elements>((HT->size)*(HT->size)))&&(operation==UP)){
        return FULL;}
    if(operation==UP)
	return 0;
    /*for(size_t i=0; i<HT->size; i++){
        if(HT->table[i].non_vacancies == HT->size && operation == UP)
            return FULL;
    }*/
    //Ahora, se evalúa si la cantidad total de elementos ocupados es menor que el cuarto de los elementos. Si es así,
    //...indicamos que está "vacía".
    //NOTA: Aquí le sumamos el cuadrado de la variable global "hist" (histéresis)
    size_t sum = 0;
    for(size_t i=0; i<HT->size; i++){
        sum += HT->table[i].len;
    }
    if((HT->occupied_elements<((sum/4)+(hist*hist)))&&(operation==DOWN)){

        //Por supuesto, si tenemos el menor tamaño posible, no mandamos "empty" para no reducir (ya no se puede)
        if((HT->index_size)>0)
            hist++;                 //Aumentamos el valor de la histéresis en 1 (cada vez que se reduzca la tabla)
            return EMPTY;
    }
    return 0;
}

/*Función para encontrar una llave en una tabla Hash con arreglos*/
hash_item* HTfindkey_SCA(HTable_SCA **HT, uint32_t key){
    //Aplicamos la función Hash
    size_t index = hashFunction(key, (*HT)->size);
    //Buscamos la llave entre todos los elementos (comparando los valores con la que acabamos de encontrar)
    for(size_t i=0; i<(*HT)->table[index].len; i++){
        hash_item item = (*HT)->table[index].elem[i];
    //Si la llave se encontró (y si no se ha marcado como "borrado"), se regresa el contenido
        if(item.status == VALID && item.key == key )
            return (&(*HT)->table[index].elem[i]);
        }
    //Si no se encontró, regresa NULL
    return NULL;
}

/*Función para encontrar un record en una tabla Hash con arreglos*/
hash_item* HTfindRecord_SCA(HTable_SCA **HT, record *rec){
    //Se calcula la llave de acuerdo al contenido
    uint32_t key = adler32((unsigned char*)rec->bytes, rec->len);               //Encuentro la llave asociada a record (una cadena de longitud "len")
    hash_item *item = HTfindkey_SCA(HT, key);
    if(item == NULL)
        return NULL;
    if(checkMatchRecord(rec, &(item->rec))==YES)
        return item;
    return NULL;
    //Se manda llamar la función de encontrar llave
    //return HTfindkey_SCA(HT, key);
}

/*Función para insertar un elemento en una tabla hash con arreglos*/
void HTinsertRecord_SCA(HTable_SCA **HT, record *rec){
    //Primeramente vamos a ver si la tabla tiene un tamaño grande. Si es así, la expandemos
    if(checkSizeSCA(*HT, UP)==FULL){
        (*HT)=RemodelHTableCap_SCA(*HT, checkSizeSCA(*HT, UP));
        //printf("Cambiamos el tamaño");
    }
    //Se calcula la llave
    uint32_t key = adler32(rec->bytes, rec->len);
    //Usando la función para encontrar una llave, se evalúa si lo que regresa es nulo o no (si no lo es, quiere decir que ya estaba el contenido...
    //... en la tabla)
    if(HTfindRecord_SCA(HT, rec) != NULL)
        return;
    //Si la ejecución llega hasta aquí, el contenido no estaba presente.
    //Primero se intenta insertar en un espacio desocupado (donde se había marcado como borrado a un elemento)
    size_t index = hashFunction(key, (*HT)->size);
    //En el AHead que corresponde, se busca entre los elementos de su arreglo algún espacio disponible
    for( size_t i=0; i<(*HT)->table[index].len; i++ ){
        hash_item *item = &((*HT)->table[index].elem[i]);
        if(item->status == NOTVALID){
            //Si estaba disponible, veamos si su contenido era nulo. Si no es así, reservamos para moverlo ahí ahí. Si era nulo, reservamos memoria desde 0
            //if(item->rec.bytes != NULL)
            //    item->rec.bytes =  realloc(item->rec.bytes, rec->len);
            //else
            item->rec.bytes = malloc(rec->len);
            item->rec.len = rec->len;
            //Agregamos la llave y marcamos como válido
            item->key = key;
            item->status = VALID;
            //Copiamos el contenido ahí
            strcpy(item->rec.bytes, rec->bytes);
            //Aumentamos el contador del total de elementos ocupados en uno
            (*HT)->occupied_elements++;
            size_t hasel = (*HT)->size;
            return;
        }
    }
    //Si no hubo un espacio disponible, se "renueva" el AHead (añadiendo el nuevo elemento en el siguiente espacio del arreglo)
    AHead newHead;
    newHead.elem = malloc(sizeof(hash_item)*((*HT)->table[index].len+1));
    newHead.len = (*HT)->table[index].len+1;
    //Colocamos todo el contenido del AHhead en su nueva versión
    for( size_t i=0; i<(*HT)->table[index].len; i++ ){
        newHead.elem[i].key = (*HT)->table[index].elem[i].key;
        newHead.elem[i].status = (*HT)->table[index].elem[i].status;
        newHead.elem[i].rec = (*HT)->table[index].elem[i].rec;
        newHead.elem[i].len = newHead.len;
    }
    //Colocamos ahora el nuevo elemento con el record a insertar
    newHead.elem[(*HT)->table[index].len].key = key;
    newHead.elem[(*HT)->table[index].len].status = VALID;
    newHead.elem[(*HT)->table[index].len].rec.bytes = malloc(rec->len); //Aquí apenas reservamos memoria
    newHead.elem[(*HT)->table[index].len].rec.len = rec->len; //Aquí definimos la longitud
    //Aquí ya copiamos el record en el nuevo elemento a usar de la tabla
    strcpy(newHead.elem[(*HT)->table[index].len].rec.bytes, rec->bytes);
    //Mantenemos el resto de los elementos como disponibles
    for( size_t i=(*HT)->table[index].len+1; i<newHead.len; i++ ){
        newHead.elem[i].key = 0;
        newHead.elem[i].status = NOTVALID;
        newHead.elem[i].rec.bytes = NULL;
        newHead.elem[i].rec.len = 0;
        newHead.elem[i].len = newHead.len;
    }
    //Liberamos el espacio de la versión anterior
    free((*HT)->table[index].elem);
    //Ponemos esta nueva versión de AHead donde corresponde
    (*HT)->table[index] = newHead;
    //Aumentamos el contador de elementos ocupados en uno
    (*HT)->occupied_elements++;
    return;
    }

//Función para borrar un record en una tabla hash con arreglos
void HTdeleteRecordSCA(HTable_SCA **HT, record *rec){
    uint32_t key = adler32(rec->bytes, rec->len);
    //Primero se busca la llave. Si era nulo, entonces no estaba (regresa a main)
    if(HTfindkey_SCA(HT, key) == NULL)
        return;
    //Si estaba la llave, se calcula la posición donde está (en el arreglo de cabezas) y se hace un barrido hasta dar con la llave
    size_t index = hashFunction(key, (*HT)->size);
    for( size_t i=0; i<(*HT)->table[index].len; i++ ){
        hash_item item = (*HT)->table[index].elem[i];
        if( item.status == VALID && item.key == key ){
            //Cuando se encuentra con la llave, se marca como "borrado"
            (*HT)->table[index].elem[i].status = NOTVALID;
            //Decrementamos el contador del total de elementos ocupados en uno
            (*HT)->occupied_elements--;
            //Finalmente vamos a ver si la tabla tiene muchos elementos sin ocupar. Si es así, la reducimos
            if(checkSizeSCA(*HT, DOWN)==EMPTY){
                if((*HT)->index_size>0){
                    (*HT)=RemodelHTableCap_SCA(*HT, checkSizeSCA(*HT, DOWN));
                    //printf("Cambiamos el tamaño");
                }
            }
            return;
        }
    }
}

/*Función para imprimir una tabla hash con arreglos*/
void HTprint_SCA(HTable_SCA *HT){
    for(size_t i=0; i<HT->size; i++){
        printf("%ld ", i);
        for(size_t j=0; j<HT->table[i].len; j++)
            HTprintItem_SC(&(HT->table[i].elem[j]));
            printf("\n");
        }
    printf("\n");
    }

//************************************INT MAIN********************************************************************************************
int main(int argc, char **argv){
    //Aquí se elige manualmente el tipo de estrategia (LL = Linked lists, A = Arrays)
    int mode;
    if(argc == 1){
        printf("Bienvenid@. Eliga la estrategia (1 = Linked lists, 0 = Arrays): ");
        scanf("%d", &mode);
    }
    else{
        mode = atoi(argv[1]);
    }
    switch(mode)
    {
    case LL:
        HTable_SC *HT = newHTable_SC();
        record rec;
        char buffer[100];
        while(fgets(buffer, 100, stdin) != NULL){
                char command[100] = " ";
                char number[100] = " ";
                sscanf(buffer, "%s %s", command, number);     //Recuerda usar el espacio para separar
                rec.bytes = number;
                rec.len = strlen(number);
                if(strcmp("insert", command)==0){               //Insertar
                    HTinsertRecord_SC(&HT, &rec);
                    continue;
                }
                if(strcmp("delete", command)==0){               //Borrar
                    HTdeleteRecord(&HT, &rec);
                    continue;
                }
                if(strcmp("stop", command)==0){                 //Parar (crea un ciclo infinito para medir memoria en servidor)
                    size_t i = 0;
                    while(i<1){
                        continue;}
                    continue;
                }
                if(strcmp("print", command)==0){                //Imprimir
                    HTprint_SC(HT);
                    continue;
                }
                if(strcmp("count", command)==0){              //Imprimir no. de elementos en la tabla
                    printf("Elementos ocupados: %ld\n", HT->occupied_elements);
		    continue;
		}
	        if(strcmp("exit", command)==0){                 //Salir
                    break;}
        }
        freeHTable_SC(HT);
        break;
    case AR:
        HTable_SCA *HT2 = newHTable_SCA();
        record rec2;
        char buffer2[100];
        while(fgets(buffer2, 100, stdin) != NULL){
                char command[100] = " ";
                char number[100] = " ";
                sscanf(buffer2, "%s %s", command, number);     //Recuerda usar el espacio para separar
                //char Data0[21];
                rec2.bytes = number;
                rec2.len = strlen(number);
                if(strcmp("insert", command)==0){
                    HTinsertRecord_SCA(&HT2, &rec2);
                    continue;
                }
                if(strcmp("delete", command)==0){
                    HTdeleteRecordSCA(&HT2, &rec2);
                    continue;
                }
                if(strcmp("print", command)==0){
                    HTprint_SCA(HT2);
                    continue;
                }
                if(strcmp("stop", command)==0){                 //Parar (crea un ciclo infinito para medir memoria en servidor)
                    size_t i = 0;
                    while(i<1){
                        continue;}
                    continue;
                }
                if(strcmp("count", command)==0){              //Imprimir no. de elementos en la tabla
		    printf("Elementos ocupados: %ld\n", HT2->occupied_elements);
		    continue;}
		
	        if(strcmp("exit", command)==0){
                    break;}
        }
        freeHTable_SCA(HT2);
        break;
    default:
        break;
    }
    printf("Gracias!\n");
    return 0;
}


