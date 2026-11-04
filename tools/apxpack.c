#include <proc/apx.h>

#include <stdio.h>
#include <string.h>

static int pack_apx(const char *input_path, const char *output_path)
{
  unsigned char elf_magic[4];
  static const unsigned char expected_elf_magic[4] = {0x7f, 'E', 'L', 'F'};
  struct apx_header header = {
    .magic = ETHOS_APX_MAGIC,
    .version = ETHOS_APX_VERSION,
    .header_size = sizeof(struct apx_header),
    .architecture = ETHOS_APX_ARCH_X86_64,
    .flags = 0
  };
  FILE *input = fopen(input_path, "rb");
  if (input == NULL)
  {
    perror(input_path);
    return 1;
  }
  if (fread(elf_magic, 1, sizeof(elf_magic), input) != sizeof(elf_magic) ||
      memcmp(elf_magic, expected_elf_magic, sizeof(elf_magic)) != 0)
  {
    fprintf(stderr, "%s: input is not an ELF executable\n", input_path);
    fclose(input);
    return 1;
  }
  rewind(input);

  FILE *output = fopen(output_path, "wb");
  if (output == NULL)
  {
    perror(output_path);
    fclose(input);
    return 1;
  }
  if (fwrite(&header, 1, sizeof(header), output) != sizeof(header))
  {
    perror(output_path);
    fclose(input);
    fclose(output);
    return 1;
  }

  unsigned char buffer[4096];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) != 0)
  {
    if (fwrite(buffer, 1, bytes_read, output) != bytes_read)
    {
      perror(output_path);
      fclose(input);
      fclose(output);
      return 1;
    }
  }
  if (ferror(input) || fclose(input) != 0 || fclose(output) != 0)
  {
    perror("APX packaging failed");
    return 1;
  }
  return 0;
}

int main(int argument_count, char *arguments[])
{
  if (argument_count != 3)
  {
    fprintf(stderr, "usage: apxpack input.elf output.apx\n");
    return 2;
  }
  return pack_apx(arguments[1], arguments[2]);
}