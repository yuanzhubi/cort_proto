#ifdef CORT_CLIENT_ECHO_INFINITE_TEST
#include <unistd.h>
#include <stdio.h>
#include "../net/cort_tcp_ctrler.h"

int timeout = 300;
int keepalive_timeout = 3000;

const char *ip = "127.0.0.1";
unsigned short port =  8888;
unsigned int connections = 50;
unsigned int send_size = 384;

char send_content[]= "From https://en.wikipedia.org/wiki/English_language: English is a West Germanic language that was first spoken in early medieval England and is now the third most widespread native language in the world, after Standard Chinese and Spanish, as well as the most widely spoken Germanic language. Named after the Angles, one of the Germanic tribes that migrated to England, it ultimately derives its name from the Anglia (Angeln) peninsula in the Baltic Sea. It is closely related to the other West Germanic languages of Frisian, Low German/Low Saxon, German, Dutch, and Afrikaans. The English vocabulary has been significantly influenced by French (a Romance language), Norse (a North Germanic language), and by Latin. English has developed over the course of more than 1,400 years. The earliest forms of English, a set of Anglo-Frisian dialects brought to Great Britain by Anglo-Saxon settlers in the 5th century, are called Old English. Middle English began in the late 11th century with the Norman conquest of England, and was a period in which the language was influenced by French.[4] Early Modern English began in the late 15th century with the introduction of the printing press to London and the King James Bible, and the start of the Great Vowel Shift.[5] Through the worldwide influence of the British Empire, modern English spread around the world from the 17th to mid-20th centuries. Through all types of printed and electronic media, as well as the emergence of the United States as a global superpower, English has become the leading language of international discourse and the lingua franca in many regions and in professional contexts such as science, navigation and law.[6] English is the most widely learned second language and is either the official language or one of the official languages in almost 60 sovereign states. There are more people who have learned it as a second language than there are native speakers. English is the most commonly spoken language in the United Kingdom, the United States, Canada, Australia, Ireland and New Zealand, and it is widely spoken in some areas of the Caribbean, Africa and South Asia.[7] It is a co-official language of the United Nations, of the European Union and of many other world and regional international organisations. It is the most widely spoken Germanic language, accounting for at least 70% of speakers of this Indo-European branch. English has a vast vocabulary, and counting exactly how many words it has is impossible.[8][9] Modern English grammar is the result of a gradual change from a typical Indo-European dependent marking pattern with a rich inflectional morphology and relatively free word order, to a mostly analytic pattern with little inflection, a fairly fixed SVO word order and a complex syntax.[10] Modern English relies more on auxiliary verbs and word order for the expression of complex tenses, aspect and mood, as well as passive constructions, interrogatives and some negation. Despite noticeable variation among the accents and dialects of English used in different countries and regions – in terms of phonetics and phonology, and sometimes also vocabulary, grammar and spelling – English-speakers from around the world are able to communicate with one another with relative ease. English is an Indo-European language, and belongs to the West Germanic group of the Germanic languages.[11] Old English originated from a Germanic tribal and linguistic continuum along the coast of the North Sea, whose languages are now known as the Anglo-Frisian subgroup within West Germanic. As such, the modern Frisian languages are the closest living relatives of Modern English. Low German/Low Saxon is also closely related, and sometimes English, the Frisian languages, and Low German are grouped together as the Ingvaeonic (North Sea Germanic) languages, though this grouping remains debated.[12] Old English evolved into Middle English, which in turn evolved into Modern English.[13] Particular dialects of Old and Middle English also developed into a number of other Anglic languages, including Scots[14] and the extinct Fingallian and Forth and Bargy (Yola) dialects of Ireland.[15] Like Icelandic and Faroese, the development of English on the British Isles isolated it from the continental Germanic languages and influences, and has since undergone substantial evolution. English is thus not mutually intelligible with any continental Germanic language, differing in vocabulary, syntax, and phonology, although some, such as Dutch or Frisian, do show strong affinities with English, especially with its earlier stages.[16] Unlike Icelandic or Faroese, the long history of invasions of the British Isles by other peoples and languages, particularly Old Norse and Norman French, left a profound mark of their own on the language, such that English shares substantial vocabulary and grammar similarities with many languages outside its linguistic clades, while also being unintelligible with any of those languages. Some scholars have even argued that English can be considered a mixed language or a creole – a theory called the Middle English creole hypothesis. Although the high degree of influence from these languages on the vocabulary and grammar of Modern English is widely acknowledged, most specialists in language contact do not consider English to be a true mixed language.[17][18] English is classified as a Germanic language because it shares innovations with other Germanic languages such as Dutch, German, and Swedish.[19] These shared innovations show that the languages have descended from a single common ancestor called Proto-Germanic. Some shared features of Germanic languages include the use of modal verbs, the division of verbs into strong and weak classes, and the sound changes affecting Proto-Indo-European consonants, known as Grimm's and Verner's laws. English is classified as an Anglo-Frisian language because Frisian and English share other features, such as the palatalisation of consonants that were velar consonants in Proto-Germanic (see Phonological history of Old English § Palatalization).[20] Proto-Germanic to Old English Main article: Old English The opening to the Old English epic poem Beowulf, handwritten in half-uncial script: Hƿæt ƿē Gārde/na ingēar dagum þēod cyninga / þrym ge frunon... 'Listen! We of the Spear-Danes from days of yore have heard of the glory of the folk-kings...' The earliest form of English is called Old English or Anglo-Saxon (c. 550–1066 CE). Old English developed from a set of North Sea Germanic dialects originally spoken along the coasts of Frisia, Lower Saxony, Jutland, and Southern Sweden by Germanic tribes known as the Angles, Saxons, and Jutes. In the fifth century, the Anglo-Saxons settled Britain as the Roman economy and administration collapsed. By the seventh century, the Germanic language of the Anglo-Saxons became dominant in Britain, replacing the languages of Roman Britain (43–409 CE): Common Brittonic, a Celtic language, and Latin, brought to Britain by the Roman occupation.[21][22][23] England and English (originally Ænglaland and Ænglisc) are named after the Angles.[24] Old English was divided into four dialects: the Anglian dialects, Mercian and Northumbrian, and the Saxon dialects, Kentish and West Saxon.[25] Through the educational reforms of King Alfred in the ninth century and the influence of the kingdom of Wessex, the West Saxon dialect became the standard written variety.[26] The epic poem Beowulf is written in West Saxon, and the earliest English poem, Cædmon's Hymn, is written in Northumbrian.[27] Modern English developed mainly from Mercian, but the Scots language developed from Northumbrian. A few short inscriptions from the early period of Old English were written using a runic script.[28] By the sixth century, a Latin alphabet was adopted, written with half-uncial letterforms. It included the runic letters wynn  and thorn, and the modified Latin letters eth, and ash .[28][29] Old English is very different from Modern English and difficult for 21st-century English speakers to understand. Its grammar was similar to that of modern German, and its closest relative is Old Frisian. Nouns, adjectives, pronouns, and verbs had many more inflectional endings and forms, and word order was much freer than in Modern English. Modern English has case forms in pronouns (he, him, his) and a few verb endings (I have, he has), but Old English had case endings in nouns as well, and verbs had more person and number endings.[30][31][32] The translation of Matthew 8:20 from 1000 CE shows examples of case endings (nominative plural, accusative plural, genitive singular) and a verb ending (present plural). In the period from the 8th to the 12th century, Old English gradually transformed through language contact into Middle English. Middle English is often arbitrarily defined as beginning with the conquest of England by William the Conqueror in 1066, but it developed further in the period from 1200–1450. First, the waves of Norse colonisation of northern parts of the British Isles in the 8th and 9th centuries put Old English into intense contact with Old Norse, a North Germanic language. Norse influence was strongest in the Northeastern varieties of Old English spoken in the Danelaw area around York, which was the centre of Norse colonisation; today these features are still particularly present in Scots and Northern English. However the centre of norsified English seems to have been in the Midlands around Lindsey, and after 920 CE when Lindsey was reincorporated into the Anglo-Saxon polity, Norse features spread from there into English varieties that had not been in intense contact with Norse speakers. Some elements of Norse influence that persist in all English varieties today are the pronouns beginning with th- (they, them, their) which replaced the Anglo-Saxon pronouns with h- (hie, him, hera).[35] With the Norman conquest of England in 1066, the now norsified Old English language was subject to contact with the Old Norman language, a Romance language closely related to Modern French. The Norman language in England eventually developed into Anglo-Norman. Because Norman was spoken primarily by the elites and nobles, while the lower classes continued speaking Anglo-Saxon, the influence of Norman consisted of introducing a wide range of loanwords related to politics, legislation and prestigious social domains.[36] Middle English also greatly simplified the inflectional system, probably in order to reconcile Old Norse and Old English, which were inflectionally different but morphologically similar. The distinction between nominative and accusative case was lost except in personal pronouns, the instrumental case was dropped, and the use of the genitive case was limited to describing possession. The inflectional system regularised many irregular inflectional forms,[37] and gradually simplified the system of agreement, making word order less flexible.[38] By the Wycliffe Bible of the 1380s, the passage Matthew 8:20 was written Foxis han dennes, and briddis of heuene han nestis[39] Here the plural suffix -n on the verb have is still retained, but none of the case endings on the nouns are present. By the 12th century Middle English was fully developed, integrating both Norse and Norman features; it continued to be spoken until the transition to early Modern English around 1500. Middle English literature includes Geoffrey Chaucer's The Canterbury Tales, and Malory's Le Morte d'Arthur. In the Middle English period the use of regional dialects in writing proliferated, and dialect traits were even used for effect by authors such as Chaucer. The next period in the history of English was Early Modern English (1500–1700). Early Modern English was characterised by the Great Vowel Shift (1350–1700), inflectional simplification, and linguistic standardisation. The Great Vowel Shift affected the stressed long vowels of Middle English. It was a chain shift, meaning that each shift triggered a subsequent shift in the vowel system. Mid and open vowels were raised, and close vowels were broken into diphthongs. For example, the word bite was originally pronounced as the word beet is today, and the second vowel in the word about was pronounced as the word boot is today. The Great Vowel Shift explains many irregularities in spelling, since English retains many spellings from Middle English, and it also explains why English vowel letters have very different pronunciations from the same letters in other languages.[40][41] English began to rise in prestige during the reign of Henry V. Around 1430, the Court of Chancery in Westminster began using English in its official documents, and a new standard form of Middle English, known as Chancery Standard, developed from the dialects of London and the East Midlands. In 1476, William Caxton introduced the printing press to England and began publishing the first printed books in London, expanding the influence of this form of English.[42] Literature from the Early Modern period includes the works of William Shakespeare and the translation of the Bible commissioned by King James I. Even after the vowel shift the language still sounded different from Modern English: for example, the consonant clusters /kn ɡn sw/ in knight, gnat, and sword were still pronounced. Many of the grammatical features that a modern reader of Shakespeare might find quaint or archaic represent the distinct characteristics of Early Modern English.[43] In the 1611 King James Version of the Bible, written in Early Modern English, Matthew 8:20 says: The Foxes haue holes and the birds of the ayre haue nests[33] This exemplifies the loss of case and its effects on sentence structure (replacement with Subject-Verb-Object word order, and the use of of instead of the non-possessive genitive), and the introduction of loanwords from French (ayre) and word replacements (bird originally meaning 'nestling' had replaced OE fugol).";

unsigned int error_count_total;
unsigned int success_count_total;
unsigned int total_time_cost;

struct errnum_counter{
    struct err_info{
        unsigned int err_cost;
        unsigned int err_times;
    };
    err_info counter[256];
    void init(){
        memset(counter, 0, sizeof(counter));
    }
    void add_error(uint8_t err, unsigned int time_cost){
        ++counter[err].err_times;
        counter[err].err_cost += time_cost;
    }
    void output(){
        for(int i = 1; i<256; ++i){
            if(counter[i].err_times != 0){
                printf("error %s: %u times, %fms averaget_time_cost!\n", 
                    cort_socket_error_codes::error_info(i), counter[i].err_times, ((double)counter[i].err_cost)/counter[i].err_times);
                counter[i].err_cost = 0;
                counter[i].err_times = 0;
            }
        }
    }
    ~errnum_counter(){
        output();
    }
    
}error_counter;

struct print_result_cort: public cort_auto_delete{
    CO_DECL(print_result_cort)
    cort_proto* start(){
        CO_BEGIN
            unsigned int total = error_count_total + success_count_total;
            if(total == 0){
                total = 1;
            }
            printf("succeed: %u, error: %u, averaget_time_cost: %fms \n", success_count_total, error_count_total, ((double)(total_time_cost))/total);
            success_count_total = 0, error_count_total = 0, total_time_cost = 0;
            error_counter.output();
        CO_END
    }
};
int64_t total_test_count = 100000000000;
struct send_cort : public cort_auto_delete{
    CO_DECL(send_cort)
    cort_tcp_request_response cort_test0;
    
    static recv_buffer_ctrl::recv_buffer_size_t recv_check_function(recv_buffer_ctrl* arg, cort_tcp_ctrler* p){
        int32_t size = p->get_recv_buffer_size();
        char* buf = p->get_recv_buffer();
        if(size == 0){
            return 0;
        }
        if(buf[size-1] == '\0'){
            return size;
        }
        return 0;
    }
    cort_proto* start(){
        CO_BEGIN
            cort_test0.set_dest_addr(ip, port);             
            cort_test0.set_timeout(timeout);
            cort_test0.set_keep_alive(keepalive_timeout);
            send_content[send_size-1] = '\0';
            cort_test0.set_send_buffer(send_content, send_size);
            cort_test0.set_recv_check_function(&send_cort::recv_check_function);
            cort_test0.alloc_recv_buffer();
            CO_AWAIT(&cort_test0);
            if(cort_test0.get_errno() != 0){
                error_counter.add_error(cort_test0.get_errno(), cort_test0.get_time_cost());
                ++error_count_total;
            }
            else{
                ++success_count_total;
            }
            total_time_cost += cort_test0.get_time_cost();
            if(--total_test_count >= 0){
                cort_test0.clear();
                return this->start();
            }                
        CO_END
    }
};

#include <sys/epoll.h>
struct stdio_switcher : public cort_fd_waiter{
    CO_DECL(stdio_switcher)
    void on_finish(){
        remove_poll_request();
        cort_timer_destroy();   //this will stop the timer loop;
        total_test_count = 0;
    }
    cort_proto* start(){
    CO_BEGIN
        set_cort_fd(0);
        set_poll_request(EPOLLIN|EPOLLHUP);
        CO_YIELD();
        if(get_poll_result() != EPOLLIN){
            puts("exception happened?");
            CO_RETURN;
        }
        char buf[1024] ;
        int result = read(0, buf, 1023);
        if(result == 0){    //using ctrl+d in *nix
            CO_RETURN;
        }
        CO_AGAIN;
    CO_END
    }
}switcher;

int main(int argc, char* argv[]){
    cort_timer_init();
    error_counter.init();
    printf( "This will start a echo client test. Press ctrl+d to stop. \n"
            "arg1: ip, default: 127.0.0.1 \n"
            "arg2: port, default: 8888 \n"
            "arg3: send_size, default: 384 \n"
            "arg4: max connection, default: 50 \n"
            "arg5: max test times, default: 100000000000 \n"
    );
    if(argc > 1){
        ip = argv[1];
    }
    if(argc > 2){
        port = (unsigned short)(atoi(argv[2]));
    }
    if(argc > 3){
        send_size = (unsigned int)(atoi(argv[3]));
    }
    if(argc > 4){
        connections = (unsigned int)(atoi(argv[4]));
    }
    if(argc > 5){
        total_test_count = (int64_t)(atoll(argv[5]));
    }
    
    while(connections-- != 0){
        (new send_cort())->start();
    }
    
    cort_repeater<print_result_cort> logger;
    logger.set_repeat_per_second(1);  //log performance 1 time per second
    logger.start();
    
    switcher.start();
    cort_timer_loop();
    cort_timer_destroy();
    return 0;   
}

#endif