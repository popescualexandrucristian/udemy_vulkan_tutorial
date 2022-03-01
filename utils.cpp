#include "utils.h"
#include <iostream>

std::vector<char> readFile(const char* filePath)
{
   std::vector<char> out;

   FILE* file = fopen(filePath, "rb");
   if (!file)
      return out;

   fseek(file, 0, SEEK_END);

   long fileSize = ftell(file);

   fseek(file, 0, SEEK_SET);

   out.resize(fileSize);

   long availableData = fileSize;
   while (availableData)
   {
      size_t receivedData = fread(out.data() + (fileSize - availableData), sizeof(char), availableData, file);
      if (ferror(file))
      {
         out.clear();
         break;
      }

      if (feof(file))
         break;

      if (receivedData > 0)
         availableData -= static_cast<long>(receivedData);
   }
   fclose(file);
   return out;
}