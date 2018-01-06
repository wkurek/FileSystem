#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 64
#define FILENAME_LENGTH 16
#define DISCNAME_LENGTH 16

#define MAX_FILES_NUMBER 16

#define EMPTY 0
#define UNUSED -1
#define USED 1

typedef struct {
  unsigned char data[BLOCK_SIZE];
  unsigned long nextBlock;
} Block;

typedef struct {
  long size;
  unsigned long firstBlock;
  char filename[FILENAME_LENGTH];
} Node;

typedef struct {
  unsigned int blocksNumber;
  unsigned int freeBlocksNumber;
  unsigned int nodesNumber;
  unsigned int freeNodesNumber;

  char name[DISCNAME_LENGTH];
  unsigned long size;

  unsigned long nodesOffset;
  unsigned long bitVectorOffset;
  unsigned long blocksOffset;
} SuperBlock;

typedef struct {
  SuperBlock superBlock;
  FILE *file;
} Disc;

SuperBlock newSuperBlockInstance(char *name, unsigned long size) {
  SuperBlock superBlock;
  strcpy(superBlock.name, name);

  superBlock.blocksNumber = (size - MAX_FILES_NUMBER*sizeof(Node) - sizeof(SuperBlock))/
    (sizeof(Block) + sizeof(int));
  superBlock.freeBlocksNumber = superBlock.blocksNumber;
  superBlock.freeNodesNumber = MAX_FILES_NUMBER;

  superBlock.nodesOffset = sizeof(SuperBlock);
  superBlock.bitVectorOffset = superBlock.nodesOffset + MAX_FILES_NUMBER*sizeof(Node);
  superBlock.blocksOffset = superBlock.bitVectorOffset + superBlock.blocksNumber*sizeof(int);

  return superBlock;
}

Node newNodeInstance() {
  Node node;
  node.size = UNUSED;
  return node;
}

Disc newDiscInstance(char *name, unsigned int size) {
  Disc disc;
  disc.superBlock = newSuperBlockInstance(name, size);
  return disc;
}

Disc disc;

int getFirstFreeBlockIndex() {
  unsigned long adress = disc.superBlock.bitVectorOffset;
  int bit, i;
  i = 0;

  do {
    fseek(disc.file, adress, 0);
    fread(&bit, sizeof(int), 1, disc.file);
    adress += sizeof(int);
    ++i;
  } while(bit == USED);

  return i-1;
}

void setBlockState(int index, int state) {
  unsigned long adress = disc.superBlock.bitVectorOffset + (index*sizeof(int));
  int bit = state;
  fseek(disc.file, adress, 0);
  fwrite(&bit, sizeof(int), 1, disc.file);
}

int getFirstFreeNodeIndex() {
  unsigned long adress = disc.superBlock.nodesOffset;
  Node node = newNodeInstance();
  int i = 0;

  do {
    fseek(disc.file, adress, 0);
    fread(&node, sizeof(Node), 1, disc.file);
    adress += sizeof(Node);
    ++i;
  } while(node.size != UNUSED);

  return i-1;
}

int openDiscFile() {
  disc.file = fopen(disc.superBlock.name, "wb+");
  if(!disc.file) return 0;

  fwrite(&disc.superBlock, sizeof(SuperBlock), 1, disc.file);

  for(int i = 0; i < MAX_FILES_NUMBER; ++i) {
    Node node = newNodeInstance();
    fwrite(&node, sizeof(Node), 1, disc.file);
  }

  for(int i = 0; i < disc.superBlock.blocksNumber; ++i) {
    int j = UNUSED;
    fwrite(&j, sizeof(int), 1, disc.file);
  }

  return 1;
}

void closeDiscFile() {
  if(disc.file){
    fclose(disc.file);
  }
}

unsigned long getFileSize(FILE *file) {
  fseek(file, 0L, SEEK_END);
  return ftell(file);
}

int isFileExistsOnDisc(char *filename) {
  unsigned long adress = disc.superBlock.nodesOffset;
  for(int i = 0; i < MAX_FILES_NUMBER; ++i) {
    Node node;
    fseek(disc.file, adress, 0);
    fread(&node, sizeof(Node), 1, disc.file);
    if(node.size != UNUSED && (strcmp(filename, node.filename) == 0)) return 1;
    adress += sizeof(Node);
  }
  return 0;
}

void copyToDisc(char *filename) {
  FILE *copyFile = fopen(filename, "rb");
  if(!copyFile) {
    printf("ERROR: cannot open file %s\n", filename);
    return;
  }

  unsigned long copyFileSize = getFileSize(copyFile);
  if(copyFileSize > disc.superBlock.freeBlocksNumber * BLOCK_SIZE) {
    printf("ERROR: not enough space on sidc to copy file %s\n", filename);
    return;
  }

  if(disc.superBlock.freeNodesNumber <= 0) {
    printf("ERROR: disc reached max number of files\n");
    return;
  }

  if(isFileExistsOnDisc(filename)) {
    printf("ERROR: file %s already exists on disc\n", filename);
    return;
  }

  int blockIndex = getFirstFreeBlockIndex();
  printf("blockIndex: %d\n", blockIndex);

  int nodeIndex = getFirstFreeNodeIndex();
  Node node;
  node.size = copyFileSize;
  node.firstBlock = blockIndex;
  strcpy(node.filename, filename);

  fseek(disc.file, disc.superBlock.nodesOffset + (nodeIndex * sizeof(Node)), 0);
  fwrite(&node, sizeof(Node), 1, disc.file);

  int copyFileSizeInBlocks = copyFileSize/BLOCK_SIZE;
  if(copyFileSize%BLOCK_SIZE != 0) copyFileSizeInBlocks+=1;

  for(int i = 0; i < copyFileSizeInBlocks; ++i) {
    Block block;
    fread(&block.data, BLOCK_SIZE, 1, copyFile);
    setBlockState(blockIndex, USED);
    int nextBlockIndex = getFirstFreeBlockIndex();
    if((i+1) == copyFileSizeInBlocks) nextBlockIndex = disc.superBlock.blocksNumber+1;
    block.nextBlock = nextBlockIndex;
    fseek(disc.file, disc.superBlock.blocksOffset + (blockIndex*sizeof(Block)), 0);
    fwrite(&block, sizeof(Block), 1, disc.file);
    blockIndex = nextBlockIndex;
    printf("blockIndex: %d\n", blockIndex);
  }

  disc.superBlock.freeNodesNumber -= 1;
  disc.superBlock.freeBlocksNumber -= copyFileSizeInBlocks;
  fseek(disc.file, 0, 0);
  fwrite(&disc.superBlock, sizeof(SuperBlock), 1, disc.file);

  fclose(copyFile);
}

int main(int argc, char** argv) {

  disc = newDiscInstance("dysk2", 100000);
  openDiscFile();
  copyToDisc("tree.jpg");
  printf("\n%lu\n", sizeof(Block));
  closeDiscFile();


  return 0;
}
