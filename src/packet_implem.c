#include "packet_interface.h"

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h> // sockaddr_in6
#include <zlib.h>
// #include <errno.h>

struct __attribute__((__packed__)) pkt {

    ptypes_t type : 2;
    uint8_t tr : 1;
    uint8_t  window : 5;
    uint16_t length : 16;
    uint8_t  seqnum : 8;
    uint32_t timestamp : 32;

    uint32_t crc1 : 32;
    char * payload;
    uint32_t crc2 : 32;
};

pkt_t* pkt_new()
{
    pkt_t *pkt = (pkt_t*) malloc(sizeof(pkt_t));
    if (pkt == NULL) {
        fprintf(stderr, "Erreur lors du malloc du package");
      return NULL;
    }
    pkt->type = 0;
    pkt->tr = 0;
    pkt->window = 0;
    pkt->length = 0;
    pkt->seqnum = 0;
    pkt->timestamp = 0;
    pkt->crc1 = 0;
    pkt->payload = NULL;
    pkt->crc2 = 0;
    return pkt;
}

void pkt_del(pkt_t *pkt)
{
    if(pkt != NULL){
      if(pkt->payload != NULL){
          free(pkt->payload);
      }
      free(pkt);
    }
}

pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt)
{
	if(!len){return E_UNCONSISTENT;}
	if (len < 6) {return E_NOHEADER;}
	if (len < 10) {return E_CRC;}
	//type
	uint8_t decoder_type = (uint8_t) (*data) >> 6;
	decoder_type = (decoder_type & (uint8_t)3);
	pkt_status_code etat=pkt_set_type( pkt,decoder_type);
	if(etat != PKT_OK){return etat;}
	//tr
	uint8_t decoder_tr = (uint8_t) (*data) >> 5 ;
	decoder_tr= (decoder_tr & (uint8_t)1);
	etat=pkt_set_tr(pkt,decoder_tr);
	if(etat != PKT_OK){return etat;}
	//window
	uint8_t decoder_window = (uint8_t) (*data);
	decoder_window =(decoder_window & (uint8_t)31);
	etat=pkt_set_window(pkt,decoder_window);
	if(etat != PKT_OK){return etat;}
	
	if (decoder_type==PTYPE_DATA){
		//Length dans un fichier PTYPE_DATA
		uint8_t *donner = (uint8_t *) data + 1;
    	uint16_t decoder_length = ntohs(*(uint16_t *) donner);
    	etat=pkt_set_length(pkt,decoder_length);
		if(etat != PKT_OK){return etat;}
		//seqnum dans un fichier PTYPE_DATA
		uint8_t decoder_seqnum = *((uint8_t *) data + 3);
		etat = pkt_set_seqnum(pkt, decoder_seqnum);
		if (etat != PKT_OK) {return etat;}
		//timestamp dans un fichier PTYPE_DATA
		uint32_t decoder_timestamp=*((uint32_t *) ((uint8_t *) data + 4));
		etat = pkt_set_timestamp(pkt, decoder_timestamp);
		if (etat != PKT_OK) {return etat;}
		//CRC1 dans un fichier PTYPE_DATA pas sûr pas sûr pas sûr pas sûr pas sûr pas sûr
		uint32_t decoder_CRC1;
		memcpy(&decoder_CRC1, data + 8, 4);
		decoder_CRC1=ntohl(decoder_CRC1);
		uint32_t CRC1_TESTER = crc32(0L, Z_NULL, 0);
		CRC1_TESTER = crc32(CRC1_TESTER, (const Bytef *)data, 8);
		if(decoder_CRC1 != CRC1_TESTER){return E_CRC;}
		etat = pkt_set_crc1(pkt, decoder_CRC1);
		if (etat != PKT_OK) {return etat;}
		
		if (decoder_tr==0){
			if(decoder_length>0) {
				//playload
				char *decoder_payload = malloc(decoder_length);
				if(!decoder_payload){return E_NOMEM;}
				memcpy(decoder_payload, (void*)&data[12], decoder_length);
				etat = pkt_set_payload(pkt, decoder_payload, decoder_length);
				if(etat != PKT_OK){return etat;}

				uint32_t decoder_CRC2;
				memcpy(&decoder_CRC2, (void*)&data[12 + decoder_length], 4);
				decoder_CRC2 = ntohl(decoder_CRC2);

				uint32_t CRC2_TESTER = crc32(0L, Z_NULL, 0);
				CRC2_TESTER = crc32(CRC2_TESTER, (const Bytef *)(&data[12]), decoder_length);
				if(decoder_CRC2 != CRC2_TESTER){return E_CRC;}

				etat = pkt_set_crc2(pkt, decoder_CRC2);
				if(etat != PKT_OK){return etat;}


				free(decoder_payload);
			}
		}else{
			etat = pkt_set_payload(pkt, NULL, 0);
			if(etat != PKT_OK){return etat;}
		}
	}
	else{
		//Length en dehors d'un fichier PTYPE_DATA (ce chant n'est pas présent dans data et dois être mis à 0.
		etat=pkt_set_length(pkt,0);
		if(etat != PKT_OK){return etat;}
		//seqnum en dehors d'un fichier PTYPE_DATA
		uint8_t decoder_seqnum = *((uint8_t *) data + 1);
		etat = pkt_set_seqnum(pkt, decoder_seqnum);
		if (etat != PKT_OK) {return etat;}
		//timestamp en dehors d'un fichier PTYPE_DATA
		uint32_t decoder_timestamp=*((uint32_t *) ((uint8_t *) data + 2));
		etat = pkt_set_timestamp(pkt, decoder_timestamp);
		if (etat != PKT_OK) {return etat;}
		//CRC1 en dehors d'un fichier PTYPE_DATA pas sûr pas sûr pas sûr pas sûr pas sûr pas sûr
		uint32_t decoder_CRC1;
		memcpy(&decoder_CRC1, data + 6, sizeof(uint32_t));
		decoder_CRC1=ntohl(decoder_CRC1);
		uint32_t CRC1_TESTER = crc32(0L, Z_NULL, 0);
		CRC1_TESTER = crc32(CRC1_TESTER, (const Bytef *)(&data[0]), 6);
		if(decoder_CRC1 != CRC1_TESTER){return E_CRC;}
		etat = pkt_set_crc1(pkt, decoder_CRC1);
		if (etat != PKT_OK) {return etat;}
	}
	return PKT_OK;
}

// l'énoncé original seble faux car c'est: pkt_status_code pkt_encode(const pkt_t*, char *buf, size_t *len)
pkt_status_code pkt_encode(const pkt_t *pkt, char *buf, size_t *len) {
    ssize_t taille_tete = predict_header_length(pkt);
    if (taille_tete < 6) {
        return E_NOHEADER;
    }

	uint8_t le_type = pkt_get_type(pkt);
	if (le_type==PTYPE_DATA){
		if (*len < pkt_get_length(pkt) + taille_tete + 2 * sizeof(uint32_t)) {return E_NOMEM;}
	}else{
		if (*len < taille_tete + sizeof(uint32_t)) {return E_NOMEM;}
	}

    uint8_t premier_byte = (pkt_get_type(pkt) << 6u) | (pkt_get_tr(pkt) << 5u) | pkt_get_window(pkt);
    memcpy(buf, &premier_byte, 1);

	if (le_type==PTYPE_DATA){
		//length
		uint16_t longueur =pkt_get_length(pkt);
		uint16_t longueur_invers = htons(longueur);
		memcpy((uint8_t *) buf + 1, &longueur_invers, 2);
		//Seqnum
		uint8_t seqnum_encode=pkt_get_seqnum(pkt);
  		memcpy(buf+3, &seqnum_encode, 1);
		//timestamp
		uint32_t timestamp_encode =  pkt_get_timestamp(pkt);
  		memcpy(buf+4, &timestamp_encode, 1);
		//crc1
		uint32_t crc1_encode = crc32(0L, Z_NULL, 0);
		crc1_encode = htonl(crc32(crc1_encode, (uint8_t *) buf, 8));
		memcpy(buf + 8, &crc1_encode, 4);
		//payload
		memcpy(buf + 12, pkt_get_payload(pkt), longueur);
		//crc2
		uint32_t crc2_encode = crc32(0L, Z_NULL, 0);
		crc2_encode = crc32(crc2_encode, (uint8_t *) (pkt_get_payload(pkt)), longueur);
		crc2_encode = htonl(crc2_encode);
		memcpy(buf + 12 + longueur, &crc2_encode, 4);

		*len = 16 + longueur;
	}else{
		//Seqnum
		uint8_t seqnum_encode=pkt_get_seqnum(pkt);
  		memcpy(buf+1, &seqnum_encode, 1);
		//timestamp
		uint32_t timestamp_encode =  pkt_get_timestamp(pkt);
  		memcpy(buf+2, &timestamp_encode, 1);
		//crc1
		uint32_t crc1_encode = crc32(0L, Z_NULL, 0);
		crc1_encode = htonl(crc32(crc1_encode, (uint8_t *) buf, 6));
		memcpy(buf + 6, &crc1_encode, 4);

		*len = 10;
	}
    return PKT_OK;
}

ptypes_t pkt_get_type  (const pkt_t* pkt)
{
	return pkt->type;
}

uint8_t  pkt_get_tr(const pkt_t* pkt)
{
	return pkt->tr;
}

uint8_t  pkt_get_window(const pkt_t* pkt)
{
	return pkt->window;
}

uint8_t  pkt_get_seqnum(const pkt_t* pkt)
{
	return pkt->seqnum;
}

uint16_t pkt_get_length(const pkt_t* pkt)
{
	if (pkt->type==PTYPE_DATA){return pkt->length;}
    return 0;
}

uint32_t pkt_get_timestamp   (const pkt_t* pkt)
{
	return pkt->timestamp;
}

uint32_t pkt_get_crc1   (const pkt_t* pkt)
{
	return pkt->crc1;
}

uint32_t pkt_get_crc2   (const pkt_t* pkt)
{
    if (pkt->type==PTYPE_DATA){
        if (pkt->tr==0){return pkt->crc2;}
    }
	return 0;
}

const char* pkt_get_payload(const pkt_t* pkt)
{
    if (pkt->type==PTYPE_DATA){
		if (pkt->tr==0){
			if(pkt->length>0){return pkt->payload;}
		}
	}
    return NULL;
}

pkt_status_code pkt_set_type(pkt_t *pkt, const ptypes_t type)
{
	if(type != PTYPE_DATA && type != PTYPE_ACK && type != PTYPE_NACK) {return E_TYPE;}
    pkt->type = type;
    return PKT_OK;
}

pkt_status_code pkt_set_tr(pkt_t *pkt, const uint8_t tr)
{
    if (tr != 0 && tr != 1) {
        return E_TR;
    }
    if ((pkt_get_type(pkt) != PTYPE_DATA) && (tr == 1))
    {
        return E_TR;
    }
    pkt->tr = tr;
    return PKT_OK;
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window)
{
    if(window > MAX_WINDOW_SIZE) {return E_WINDOW;}
    pkt->window = window;
    return PKT_OK;
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum)
{
	pkt->seqnum = seqnum;
	return PKT_OK;
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length)
{
	if(length > MAX_PAYLOAD_SIZE) {return E_LENGTH;}
    pkt->length = length;//pas besoin de la fonction htons, car je le fait déjà dans pkt_decode
    return PKT_OK;
}

pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp)
{
    pkt->timestamp = timestamp;
    return PKT_OK;
}

pkt_status_code pkt_set_crc1(pkt_t *pkt, const uint32_t crc1)
{
    pkt->crc1 = crc1;
    return PKT_OK;
}

pkt_status_code pkt_set_payload(pkt_t *pkt, const char *data, const uint16_t length)
{
    pkt_status_code status = pkt_set_length(pkt, length);
    if(pkt_get_payload(pkt) != NULL){free(pkt->payload);}
    if(status != PKT_OK){return status;}
    pkt->payload = (char*) malloc(length);
    if(pkt->payload==NULL){
        return E_NOMEM;}
    memcpy(pkt->payload, data, length);
    return PKT_OK;
}

pkt_status_code pkt_set_crc2(pkt_t *pkt, const uint32_t crc2)
{
    pkt->crc2 = crc2;
    return PKT_OK;
}

ssize_t predict_header_length(const pkt_t *pkt)
{
    if (pkt_get_type(pkt)==PTYPE_DATA){
		return 8;
    }else if(pkt_get_type(pkt)==PTYPE_ACK || pkt_get_type(pkt)==PTYPE_NACK){
		if (pkt_get_length(pkt)==0){
			return 6;
		}
    }
	return -1;
}