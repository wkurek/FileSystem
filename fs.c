#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 256
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

SuperBlock newSuperBlockInstance(unsigned long size) {
  SuperBlock superBlock;
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

Disc newDiscInstance(unsigned int size) {
  Disc disc;
  disc.superBlock = newSuperBlockInstance(size);
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

int openDiscFile(char *filename) {
  disc.file = fopen(filename, "rb+");
  if(!disc.file) return 0;

  fread(&disc.superBlock, sizeof(SuperBlock), 1, disc.file);
  printf("SuperBlock freeNodesNumber:\t%d\n", disc.superBlock.freeNodesNumber);

  return 1;
}

int createDiscFile(char *filename) {
  strcpy(disc.superBlock.name, filename);

  disc.file = fopen(disc.superBlock.name, "wb+");
  if(!disc.file) return 0;
  unsigned long adress = 0;

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

void deleteDisc() {
  if(remove(disc.superBlock.name) == 0) {
    printf("Successfully deleted disc: %s\n", disc.superBlock.name);
  } else {
    printf("Cannot delete disc: %s\n", disc.superBlock.name);
  }
}

unsigned long getFileSize(FILE *file) {
  fseek(file, 0L, SEEK_END);
  unsigned long size = ftell(file);
  fseek(file, 0, 0);
  return size;
}

unsigned long getFileSizeOnDisc(char *filename) {
  unsigned long adress = disc.superBlock.nodesOffset;
  for(int i = 0; i < MAX_FILES_NUMBER; ++i) {
    Node node;
    fseek(disc.file, adress, 0);
    fread(&node, sizeof(Node), 1, disc.file);

    if(node.size != UNUSED && (strcmp(filename, node.filename) == 0))
      return node.size;

    adress += sizeof(Node);
  }
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

long getFirstBlockIndexOfFile(char *filename) {
  unsigned long adress = disc.superBlock.nodesOffset;
  for(int i = 0; i < MAX_FILES_NUMBER; ++i) {
    Node node;
    fseek(disc.file, adress, 0);
    fread(&node, sizeof(Node), 1, disc.file);

    if(node.size != UNUSED && (strcmp(filename, node.filename) == 0))
      return node.firstBlock;

    adress += sizeof(Node);
  }
}

void copyToDisc(char *filename) {
  FILE *copyFile = fopen(filename, "rb");
  if(!copyFile) {
    printf("ERROR: cannot open file %s\n", filename);
    return;
  }

  unsigned long copyFileSize = getFileSize(copyFile);
  if(copyFileSize > disc.superBlock.freeBlocksNumber * BLOCK_SIZE) {
    printf("ERROR: not enough space on disc to copy file %s\n", filename);
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
    fread(block.data, BLOCK_SIZE, 1, copyFile);
    setBlockState(blockIndex, USED);
    int nextBlockIndex = getFirstFreeBlockIndex();
    if((i+1) == copyFileSizeInBlocks) nextBlockIndex = disc.superBlock.blocksNumber+1;
    block.nextBlock = nextBlockIndex;
    fseek(disc.file, disc.superBlock.blocksOffset + (blockIndex*sizeof(Block)), 0);
    fwrite(&block, sizeof(Block), 1, disc.file);
    blockIndex = nextBlockIndex;
  }

  disc.superBlock.freeNodesNumber -= 1;
  disc.superBlock.freeBlocksNumber -= copyFileSizeInBlocks;
  fseek(disc.file, 0, 0);
  fwrite(&disc.superBlock, sizeof(SuperBlock), 1, disc.file);

  fclose(copyFile);
}

void copyFromDisc(char *filename) {
  if(!isFileExistsOnDisc(filename)) {
    printf("ERROR: no such file: %s on disc\n", filename);
    return;
  }

  FILE *destinationFile = fopen("drzewo.jpg", "wb+"); //TODO: change fixed name of file
  if(!destinationFile) {
    printf("ERROR: cannot create file %s\n", filename);
    return;
  }

  long size = getFileSizeOnDisc(filename);
  long blockIndex = getFirstBlockIndexOfFile(filename);
  Block block;

  do {
    fseek(disc.file, disc.superBlock.blocksOffset + (blockIndex*sizeof(Block)), 0);
    fread(&block, sizeof(Block), 1, disc.file);
    size -= BLOCK_SIZE;

    if(size < 0) fwrite(block.data, BLOCK_SIZE + size, 1, destinationFile);
    else fwrite(block.data, BLOCK_SIZE, 1, destinationFile);

    blockIndex = block.nextBlock;
  } while(blockIndex != disc.superBlock.blocksNumber+1);

  fclose(destinationFile);
}

void deleteNode(char *filename) {
  unsigned long adress = disc.superBlock.nodesOffset;
  for(int i = 0; i < MAX_FILES_NUMBER; ++i) {
    Node node;
    fseek(disc.file, adress, 0);
    fread(&node, sizeof(Node), 1, disc.file);

    if(node.size != UNUSED && (strcmp(filename, node.filename) == 0)){
      node.size = UNUSED;
      fseek(disc.file, adress, 0);
      fwrite(&node, sizeof(Node), 1, disc.file);
      return;
    }

    adress += sizeof(Node);
  }
}

void deleteFileFromDisc(char *filename) {
  if(!isFileExistsOnDisc(filename)) {
    printf("ERROR: no such file: %s on disc\n", filename);
    return;
  }

  long blockIndex = getFirstBlockIndexOfFile(filename);
  long sizeInBlocks = 0;

  do {
    Block block;
    fseek(disc.file, disc.superBlock.blocksOffset + (blockIndex*sizeof(Block)), 0);
    fread(&block, sizeof(Block), 1, disc.file);

    setBlockState(blockIndex, UNUSED);

    blockIndex = block.nextBlock;
    ++sizeInBlocks;
  } while(blockIndex != disc.superBlock.blocksNumber+1);

  deleteNode(filename);

  disc.superBlock.freeNodesNumber += 1;
  disc.superBlock.freeBlocksNumber += sizeInBlocks;
  fseek(disc.file, 0, 0);
  fwrite(&disc.superBlock, sizeof(SuperBlock), 1, disc.file);

}

void listDisc() {
  if(disc.superBlock.freeNodesNumber == MAX_FILES_NUMBER) return; //no files on disc

  unsigned long adress = disc.superBlock.nodesOffset;
  for(int i = 0; i < MAX_FILES_NUMBER; ++i) {
    Node node;
    fseek(disc.file, adress, 0);
    fread(&node, sizeof(Node), 1, disc.file);
    if(node.size != UNUSED) {
      printf("%s\n", node.filename);
    }
    adress += sizeof(Node);
  }
}

void printDiscMap() {
  unsigned long adress = 0;

  SuperBlock superBlock;
  fseek(disc.file, adress, 0);
  fread(&superBlock, sizeof(SuperBlock), 1, disc.file);
  printf("[%lu]\t%s\t%lu\t%d\n", adress, "SuperBlock", sizeof(SuperBlock), USED);

  adress = disc.superBlock.nodesOffset;
  for(int i = 0; i < MAX_FILES_NUMBER; ++i) {
    Node node;
    fseek(disc.file, adress, 0);
    fread(&node, sizeof(Node), 1, disc.file);
    int used = node.size != UNUSED ? 1 : 0;
    printf("[%lu]\t%s\t\t%lu\t%d\n", adress, "Node", sizeof(Node), used);
    adress += sizeof(Node);
  }

  printf("[%lu]\t%s\t%lu\t%d\n", adress, "BitVector",
      disc.superBlock.blocksNumber*sizeof(int), 1);

  adress = disc.superBlock.bitVectorOffset;
  for(int i = 0; i < disc.superBlock.blocksNumber; ++i) {
    int used;
    fseek(disc.file, adress, 0);
    fread(&used, sizeof(int), 1, disc.file);
    if(used == UNUSED) used = 0;
    printf("[%lu]\t%s\t\t%lu\t%d\n", disc.superBlock.blocksOffset+(i*sizeof(Block)),
    "Block", sizeof(Block), used);
    adress += sizeof(int);
  }
}


int main(int argc, char** argv) {
  //disc = newDiscInstance(100000);
  openDiscFile("dysk3");
  copyToDisc("tree.jpg");
  copyToDisc("tree2.jpg");
  listDisc();
  closeDiscFile();


  //disc = newDiscInstance("dysk2", 100000);
  //openDiscFile("dysk2");
  //listDisc();
  /*copyToDisc("tree.jpg");
  copyToDisc("tree2.jpg");
  copyToDisc("a.txt");
  //listDisc();
  deleteFileFromDisc("tree.jpg");
  copyToDisc("test0.txt");
  copyToDisc("tree2.jpg");
  copyFromDisc("tree2.jpg");
  //listDisc();
  listDisc();
  printDiscMap();*/



  return 0;
}
