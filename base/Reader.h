#ifndef READER_H
#define READER_H
#include "Setting.h"
#include "Triple.h"
#include <cstdlib>
#include <algorithm>

INT *freqRel, *freqEnt;
INT *lefHead, *rigHead;
INT *lefTail, *rigTail;
INT *lefRel, *rigRel;
REAL *left_mean, *right_mean;

Triple *trainList;
Triple *trainHead;
Triple *trainTail;
Triple *trainRel;

INT *testLef, *testRig;
INT *validLef, *validRig;

extern "C"
void importTrainFiles() {

	printf("The toolkit is importing datasets.\n");
	FILE *fin;
	int tmp;

	fin = fopen((inPath + "relation2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%ld", &relationTotal);
	printf("The total of relations is %ld.\n", relationTotal);
	fclose(fin);

	fin = fopen((inPath + "entity2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%ld", &entityTotal);
	printf("The total of entities is %ld.\n", entityTotal);
	fclose(fin);

	fin = fopen((inPath + "train2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%ld", &trainTotal);
	trainList = (Triple *)calloc(trainTotal, sizeof(Triple));
	trainHead = (Triple *)calloc(trainTotal, sizeof(Triple));
	trainTail = (Triple *)calloc(trainTotal, sizeof(Triple));
	trainRel = (Triple *)calloc(trainTotal, sizeof(Triple));
	freqRel = (INT *)calloc(relationTotal, sizeof(INT)); // freqRel[r_i] is the number of times the relation r_i was seen in trainList (this is correct)
	freqEnt = (INT *)calloc(entityTotal, sizeof(INT));
	for (INT i = 0; i < trainTotal; i++) {
		tmp = fscanf(fin, "%ld", &trainList[i].h);
		tmp = fscanf(fin, "%ld", &trainList[i].t);
		tmp = fscanf(fin, "%ld", &trainList[i].r);
	}
	fclose(fin);
	std::sort(trainList, trainList + trainTotal, Triple::cmp_head);
	tmp = trainTotal; trainTotal = 1;
	trainHead[0] = trainTail[0] = trainRel[0] = trainList[0];
	freqEnt[trainList[0].t] += 1;
	freqEnt[trainList[0].h] += 1;
	freqRel[trainList[0].r] += 1;
	for (INT i = 1; i < tmp; i++)
		if (trainList[i].h != trainList[i - 1].h || trainList[i].r != trainList[i - 1].r || trainList[i].t != trainList[i - 1].t) {
			trainHead[trainTotal] = trainTail[trainTotal] = trainRel[trainTotal] = trainList[trainTotal] = trainList[i];
			trainTotal++;
			freqEnt[trainList[i].t]++;
			freqEnt[trainList[i].h]++;
			freqRel[trainList[i].r]++;
		}

	// block below stores the training data into three equivalent lists,
	// that differ by their sorting.
	std::sort(trainHead, trainHead + trainTotal, Triple::cmp_head); // sorted by head entitiy (and then relation, and then tail)
	std::sort(trainTail, trainTail + trainTotal, Triple::cmp_tail); // sorted by tail entity (and then relation, and then head)
	std::sort(trainRel, trainRel + trainTotal, Triple::cmp_rel); // sorted by head entity (and then tail, and then relation)
	printf("The total of train triples is %ld.\n", trainTotal);

	// //--------------------------------------
	// printf("\n\n*** Starting to print DEBUG ***\n\n");
	// for (INT i = 0; i < trainTotal; i++) {
	// 	printf("(%li, %li, %li)\n", trainHead[i].h, trainHead[i].r, trainHead[i].t);
	// }
	// printf("----------------\n");
	// for (INT i = 0; i < trainTotal; i++) {
	// 	printf("(%li, %li, %li)\n", trainRel[i].h, trainRel[i].r, trainRel[i].t);
	// }
	// printf("\n*** End of print DEBUG ***\n\n\n");
	// //--------------------------------------

	lefHead = (INT *)calloc(entityTotal, sizeof(INT)); // lefHead[e_i] is the first index for triples whose head is e_i in trainHead
	rigHead = (INT *)calloc(entityTotal, sizeof(INT)); // rigHead[e_i] is the last index for triples whose head is e_i in trainHead
	lefTail = (INT *)calloc(entityTotal, sizeof(INT)); // lefTail[e_i] is the first index for triples whose tail is e_i in trainTail
	rigTail = (INT *)calloc(entityTotal, sizeof(INT)); // rigTail[e_i] is the last index for triples whose tail is e_i in trainTail
	lefRel = (INT *)calloc(entityTotal, sizeof(INT)); // lefRel[e_i] is the first index for triples whose head is e_i in trainRel
	rigRel = (INT *)calloc(entityTotal, sizeof(INT)); // rigRel[e_i] is the last index for triples whose head is e_i in trainRel
	memset(rigHead, -1, sizeof(INT)*entityTotal);
	memset(rigTail, -1, sizeof(INT)*entityTotal);
	memset(rigRel, -1, sizeof(INT)*entityTotal);
	for (INT i = 1; i < trainTotal; i++) {
		if (trainTail[i].t != trainTail[i - 1].t) {
			rigTail[trainTail[i - 1].t] = i - 1;
			lefTail[trainTail[i].t] = i;
		}
		if (trainHead[i].h != trainHead[i - 1].h) {
			rigHead[trainHead[i - 1].h] = i - 1;
			lefHead[trainHead[i].h] = i;
		}
		if (trainRel[i].h != trainRel[i - 1].h) {
			rigRel[trainRel[i - 1].h] = i - 1;
			lefRel[trainRel[i].h] = i;
		}
	}
	lefHead[trainHead[0].h] = 0;
	rigHead[trainHead[trainTotal - 1].h] = trainTotal - 1;
	lefTail[trainTail[0].t] = 0;
	rigTail[trainTail[trainTotal - 1].t] = trainTotal - 1;
	lefRel[trainRel[0].h] = 0;
	rigRel[trainRel[trainTotal - 1].h] = trainTotal - 1;

	left_mean = (REAL *)calloc(relationTotal,sizeof(REAL)); // left_mean[r_i] corresponds to tail_per_head of r_i (tph[r_i])
	right_mean = (REAL *)calloc(relationTotal,sizeof(REAL)); // right_mean[r_i] corresponds to head_per_tail of r_i (hpt[r_i])
	for (INT i = 0; i < entityTotal; i++) {
		for (INT j = lefHead[i] + 1; j <= rigHead[i]; j++)
			if (trainHead[j].r != trainHead[j - 1].r)
				left_mean[trainHead[j].r] += 1.0;
		if (lefHead[i] <= rigHead[i])
			left_mean[trainHead[lefHead[i]].r] += 1.0;
		for (INT j = lefTail[i] + 1; j <= rigTail[i]; j++)
			if (trainTail[j].r != trainTail[j - 1].r)
				right_mean[trainTail[j].r] += 1.0;
		if (lefTail[i] <= rigTail[i])
			right_mean[trainTail[lefTail[i]].r] += 1.0;
	}
	for (INT i = 0; i < relationTotal; i++) {
		left_mean[i] = freqRel[i] / left_mean[i]; // left_mean[r_i] = #_triples_with_r_i / #_different_heads_in_r_i
		right_mean[i] = freqRel[i] / right_mean[i]; // right_mean[r_i] = #_triples_with_r_i / #_different_tails_in_r_i
	}
}

Triple *testList;
Triple *validList;
Triple *tripleList;

extern "C"
void importTestFiles() {
    FILE *fin;
    INT tmp;

	fin = fopen((inPath + "relation2id.txt").c_str(), "r");
    tmp = fscanf(fin, "%ld", &relationTotal);
    fclose(fin);

	fin = fopen((inPath + "entity2id.txt").c_str(), "r");
    tmp = fscanf(fin, "%ld", &entityTotal);
    fclose(fin);

    FILE* f_kb1 = fopen((inPath + "test2id.txt").c_str(), "r");
    FILE* f_kb2 = fopen((inPath + "train2id.txt").c_str(), "r");
    FILE* f_kb3 = fopen((inPath + "valid2id.txt").c_str(), "r");
    tmp = fscanf(f_kb1, "%ld", &testTotal);
    tmp = fscanf(f_kb2, "%ld", &trainTotal);
    tmp = fscanf(f_kb3, "%ld", &validTotal);
    tripleTotal = testTotal + trainTotal + validTotal;
    testList = (Triple *)calloc(testTotal, sizeof(Triple));
    validList = (Triple *)calloc(validTotal, sizeof(Triple));
    tripleList = (Triple *)calloc(tripleTotal, sizeof(Triple));
    for (INT i = 0; i < testTotal; i++) {
        tmp = fscanf(f_kb1, "%ld", &testList[i].h);
        tmp = fscanf(f_kb1, "%ld", &testList[i].t);
        tmp = fscanf(f_kb1, "%ld", &testList[i].r);
        tripleList[i] = testList[i];
    }
    for (INT i = 0; i < trainTotal; i++) {
        tmp = fscanf(f_kb2, "%ld", &tripleList[i + testTotal].h);
        tmp = fscanf(f_kb2, "%ld", &tripleList[i + testTotal].t);
        tmp = fscanf(f_kb2, "%ld", &tripleList[i + testTotal].r);
    }
    for (INT i = 0; i < validTotal; i++) {
        tmp = fscanf(f_kb3, "%ld", &tripleList[i + testTotal + trainTotal].h);
        tmp = fscanf(f_kb3, "%ld", &tripleList[i + testTotal + trainTotal].t);
        tmp = fscanf(f_kb3, "%ld", &tripleList[i + testTotal + trainTotal].r);
        validList[i] = tripleList[i + testTotal + trainTotal];
    }
    fclose(f_kb1);
    fclose(f_kb2);
    fclose(f_kb3);

    std::sort(tripleList, tripleList + tripleTotal, Triple::cmp_head); // sorted by head entity (and then relation, and then tail)
    std::sort(testList, testList + testTotal, Triple::cmp_rel2); // sorted by relation (and then head, and then tail)
    std::sort(validList, validList + validTotal, Triple::cmp_rel2); // sorted by relation (and then head, and then tail)
    printf("The total of test triples is %ld.\n", testTotal);
    printf("The total of valid triples is %ld.\n", validTotal);

    testLef = (INT *)calloc(relationTotal, sizeof(INT));
	testRig = (INT *)calloc(relationTotal, sizeof(INT));
	memset(testLef, -1, sizeof(INT)*relationTotal);
	memset(testRig, -1, sizeof(INT)*relationTotal);
	for (INT i = 1; i < testTotal; i++) {
		if (testList[i].r != testList[i-1].r) {
			testRig[testList[i-1].r] = i - 1;
			testLef[testList[i].r] = i;
		}
	}
	testLef[testList[0].r] = 0;
	testRig[testList[testTotal - 1].r] = testTotal - 1;


	validLef = (INT *)calloc(relationTotal, sizeof(INT));
	validRig = (INT *)calloc(relationTotal, sizeof(INT));
	memset(validLef, -1, sizeof(INT)*relationTotal);
	memset(validRig, -1, sizeof(INT)*relationTotal);
	for (INT i = 1; i < validTotal; i++) {
		if (validList[i].r != validList[i-1].r) {
			validRig[validList[i-1].r] = i - 1;
			validLef[validList[i].r] = i;
		}
	}
	validLef[validList[0].r] = 0;
	validRig[validList[validTotal - 1].r] = validTotal - 1;
}


INT head_lef[10000];
INT head_rig[10000];
INT tail_lef[10000];
INT tail_rig[10000];
INT head_type[1000000];
INT tail_type[1000000];

extern "C"
void importTypeFiles() {
    // the type_constrain.txt file is a file that have two lines for each relation: the first line references all entities that are head of the relation; the second one references all entities that are tail.
    //     - column 1 contains the relation id
    //     - column 2 contains the number of entities that are head (or tail)
    //     - column 3 and beyond contain the entities ids
	INT total_lef = 0;
    INT total_rig = 0;
    FILE* f_type = fopen((inPath + "type_constrain.txt").c_str(),"r");
    INT tmp;
    tmp = fscanf(f_type, "%ld", &tmp); // gets rid of the first line (which has only one floating value)
    for (INT i = 0; i < relationTotal; i++) {
        INT rel, tot;
        tmp = fscanf(f_type, "%ld%ld", &rel, &tot); // reads first (relation) and second (number of entities) columns
        head_lef[rel] = total_lef; // `head_lef` stores for each relation the first index of its entities type in `head_type`
        for (INT j = 0; j < tot; j++) {
            tmp = fscanf(f_type, "%ld", &head_type[total_lef]);
            total_lef++;
        }
        head_rig[rel] = total_lef; // `head_rig` stores for each relation the last index of its entities in `head_type`
        std::sort(head_type + head_lef[rel], head_type + head_rig[rel]);
        // head_type stores the entity ids that are head of a relation. Which relation it is is accessible through the `head_lef` and `head_rig` described above.

        tmp = fscanf(f_type, "%ld%ld", &rel, &tot);
        tail_lef[rel] = total_rig;
        for (INT j = 0; j < tot; j++) {
            tmp = fscanf(f_type, "%ld", &tail_type[total_rig]);
            total_rig++;
        }
        tail_rig[rel] = total_rig;
        std::sort(tail_type + tail_lef[rel], tail_type + tail_rig[rel]);
    }
    fclose(f_type);
}


#endif
