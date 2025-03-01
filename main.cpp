// Croitoru Constantin-Bogdan 334CA
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

// functie de hash pentru perechea dintre string si int
struct MyHash {
    size_t operator()(const pair<string, int> &p) const {
        size_t hash1 = hash<string>{}(p.first);
        size_t hash2 = hash<int>{}(p.second);
        size_t hash = hash1 ^ hash2;
        return hash;
    }
};

struct my_struct {
    int id; // id-ul threadului
    vector<string> file_names;  // vector cu numele fisierelor
    int nr_files;  // numarul de fisiere din vector
    int M; // numarul de threaduri Mapper
    int R; // numarul de threaduri Reducer
    vector<unordered_map<char, unordered_set<pair<string, int>, MyHash>>> *words_vector;  // vector de mapuri de perechi cuvant id_fisier
    int *nr_files_read; // numarul de fisiere citite

    pthread_mutex_t *mutex;      // mutex
    pthread_barrier_t *barrier;  // bariera
    int *nr_letter;              // numarul de litere procesate, adica a e 0, b e 1,...
};

// extrag argumentele din linia de comanda
void get_args(int argc, char **argv, int &M, int &R, char *file_name) {
    if (argc < 4) {
        printf("Numar insuficient de parametri\n");
        exit(1);
    }

    // numarul de threaduri Mapper
    M = atoi(argv[1]);
    // numarul de threaduri Reducer
    R = atoi(argv[2]);
    strcpy(file_name, argv[3]);
}

// modific cuvantul pentru a avea doar litere mici , fara alte caractere
void verif_word(char *word) {
    // parcurg fiecare litera din cuvant si o verific
    for (size_t i = 0; i < strlen(word); i++) {
        if ('A' <= word[i] && word[i] <= 'Z')
            word[i] = word[i] + 32;
        else if (!('a' <= word[i] && word[i] <= 'z')) {
            for (size_t j = i; j < strlen(word); j++)
                word[j] = word[j + 1];
            i--;
        }
    }
}

// functie pentru threadurile Mapper
void *thread_func_Maper(void *arg) {
    my_struct *struct_for_thread = (my_struct *)arg;

    pthread_mutex_lock(struct_for_thread->mutex);
    int nr = *(struct_for_thread->nr_files_read);  // nr reprezinta fisierul la care sunt
    *(struct_for_thread->nr_files_read) = nr + 1;  // cresc numarul de fisiere citite
    pthread_mutex_unlock(struct_for_thread->mutex);

    // referinta catre vectorul de mapuri de perechi cuvant id_fisier
    vector<unordered_map<char, unordered_set<pair<string, int>, MyHash>>> &words_vector = *(struct_for_thread->words_vector);
    // referinta catre mapul de perechi din vector
    unordered_map<char, unordered_set<pair<string, int>, MyHash>> &words = words_vector[struct_for_thread->id];

    while (nr < struct_for_thread->nr_files) {
        // deschid fisierul pe care trebuie sa il citesc
        FILE *f = fopen(struct_for_thread->file_names[nr].c_str(), "r");
        if (f == NULL) {
            printf("Fisierul %s nu a putut fi deschis\n", struct_for_thread->file_names[nr].c_str());
            exit(1);
        }

        // citesc fiecare cuvant din fisier
        char word[100];
        while (fscanf(f, "%s", word) != EOF) {
            verif_word(word);
            // adaugat cuvantul in map avand cheia prima litera a cuvantului si valoarea o pereche intre cuvant si id-ul fisierului
            words[word[0]].insert(make_pair(word, nr + 1));
        }

        fclose(f);

        pthread_mutex_lock(struct_for_thread->mutex);
        nr = *(struct_for_thread->nr_files_read);
        *(struct_for_thread->nr_files_read) = nr + 1;  // cresc numarul de fisiere citite
        pthread_mutex_unlock(struct_for_thread->mutex);
    }

    pthread_barrier_wait(struct_for_thread->barrier);
    pthread_exit(NULL);
}

// cod pentru compararea cuvintelor
bool my_comparator(pair<string, list<int>> a, pair<string, list<int>> b) {
    // daca au acelasi numar de fisiere in care apar, le sortez alfabetic
    if (a.second.size() == b.second.size())
        return a.first < b.first;
    return a.second.size() > b.second.size();
}

// functie pentru threadurile Reducer
void *thread_func_Reducer(void *arg) {
    my_struct *struct_for_thread = (my_struct *)arg;

    // astept sa se termine toate threadurile Mapper ca sa pot procesa datele
    pthread_barrier_wait(struct_for_thread->barrier);

    // in partial list retin perechi cuvant lista de id_fisiere din vectorul de mapuri care incep cu o anumita litera
    // initial lista de id-uri are doar un element, iar in final _list combin id-urile pentru acelasi cuvant
    vector<pair<string, list<int>>> partial_list;
    // retin perechi cuvant lista id_fisiere
    vector<pair<string, list<int>>> final_list;

    // litera reprezinta litera pe care o procesez, adica a e 0, b e 1,...
    // literele le procesez in ordine alfabetica
    pthread_mutex_lock(struct_for_thread->mutex);
    int litera = *(struct_for_thread->nr_letter);
    *(struct_for_thread->nr_letter) = litera + 1;
    pthread_mutex_unlock(struct_for_thread->mutex);

    // cat timp mai am litere de procesat
    while (litera < 26) {
        partial_list.clear();
        for (long unsigned int i = 0; i < (*struct_for_thread->words_vector).size(); i++) {
            unordered_map<char, unordered_set<pair<string, int>, MyHash>> &aux = (*struct_for_thread->words_vector)[i];
            unordered_set<pair<string, int>, MyHash> &initial_list = aux[litera + 'a'];

            for (auto &a : initial_list) {
                list<int> l;
                l.push_back(a.second);
                partial_list.push_back(make_pair(a.first, l));
            }
        }

        // sortez lista partiala
        sort(partial_list.begin(), partial_list.end(), my_comparator);

        final_list.clear();

        // combin indexii pentru aceleasi cuvinte, de exemplu daca am cuvantul {"bogdan", {1}} si {"bogdan", {2}}, fac {"bogdan", {1,2}}
        if (partial_list.size() <= 2) {
            for (auto &a : partial_list) {
                bool ok = false;
                for (auto &b : final_list) {
                    if (a.first == b.first) {
                        b.second.merge(a.second);
                        ok = true;
                        break;
                    }
                }
                if (!ok) {
                    final_list.push_back(a);
                }
            }
        } else {
            for (long unsigned int i = 0; i < partial_list.size() - 1; i++) {
                if (partial_list[i].first == partial_list[i + 1].first) {
                    partial_list[i + 1].second.merge(partial_list[i].second);
                } else {
                    final_list.push_back(partial_list[i]);
                }
            }

            // daca ultimele 2 sunt la fel, le combin
            if (partial_list[partial_list.size() - 1].first == partial_list[partial_list.size() - 2].first) {
                partial_list[partial_list.size() - 1].second.merge(partial_list[partial_list.size() - 2].second);
                final_list.push_back(partial_list[partial_list.size() - 1]);
            } else {
                final_list.push_back(partial_list[partial_list.size() - 1]);
            }
        }
        // sortez lista finala
        sort(final_list.begin(), final_list.end(), my_comparator);

        // creez numele fisierului
        char file_name[100] = "x.txt\0";
        char c = 'a' + litera;
        file_name[0] = c;

        FILE *f = fopen(file_name, "w");
        if (f == NULL) {
            printf("Fisierul %s nu a putut fi deschis\n", file_name);
            exit(1);
        }

        // scriu in fisier
        bool ok = false;
        for (auto &a : final_list) {
            fprintf(f, "%s:[", a.first.c_str());
            ok = false;
            for (auto &b : a.second) {
                if (!ok) {
                    fprintf(f, "%d", b);
                    ok = true;
                } else
                    fprintf(f, " %d", b);
            }

            fprintf(f, "]\n");
        }

        // inchid fisierul
        fclose(f);

        // trec la urmatoarea litera
        pthread_mutex_lock(struct_for_thread->mutex);
        litera = (*struct_for_thread->nr_letter);
        (*struct_for_thread->nr_letter) = litera + 1;
        pthread_mutex_unlock(struct_for_thread->mutex);
    }

    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    int M, R, i;
    char file_name[100];
    // extrag argumentele din linia de comanda
    get_args(argc, argv, M, R, file_name);

    // deschid fisierul
    FILE *f = fopen(file_name, "r");
    if (f == NULL) {
        printf("Fisierul %s nu a putut fi deschis\n", file_name);
        exit(1);
    }

    // vector cu numele fisierelor
    vector<string> file_names;
    int nr_files;
    fscanf(f, "%d", &nr_files);
    for (int i = 0; i < nr_files; i++) {
        char file_name[100];
        fscanf(f, "%s", file_name);
        file_names.push_back(file_name);
    }

    // inchid fisierul
    fclose(f);

    vector<unordered_map<char, unordered_set<pair<string, int>, MyHash>>> words_vector;
    for (int i = 0; i < M; i++) {
        words_vector.push_back(unordered_map<char, unordered_set<pair<string, int>, MyHash>>());
    }

    int nr_files_read = 0;
    int nr_letter = 0;
    pthread_mutex_t mutex;
    pthread_barrier_t barrier;
    pthread_mutex_init(&mutex, NULL);
    pthread_barrier_init(&barrier, NULL, M + R);

    pthread_t tid[M + R];

    my_struct my_struct[M + R];
    for (i = 0; i < M + R; i++) {
        my_struct[i].id = i;
        my_struct[i].file_names = file_names;
        my_struct[i].nr_files = nr_files;
        my_struct[i].M = M;
        my_struct[i].R = R;
        my_struct[i].nr_files_read = &nr_files_read;
        my_struct[i].nr_letter = &nr_letter;
        my_struct[i].words_vector = &words_vector;
        my_struct[i].mutex = &mutex;
        my_struct[i].barrier = &barrier;

        if (i < M)
            pthread_create(&tid[i], NULL, thread_func_Maper, &my_struct[i]);
        else
            pthread_create(&tid[i], NULL, thread_func_Reducer, &my_struct[i]);
    }

    for (i = 0; i < M + R; i++) {
        pthread_join(tid[i], NULL);
    }

    // distrung mutexul si bariera
    pthread_mutex_destroy(&mutex);
    pthread_barrier_destroy(&barrier);

    return 0;
}