#ifndef V4R_IO_FILESYSTEM__H_
#define V4R_IO_FILESYSTEM__H_

#include <v4r/core/macros.h>

#include <string>
#include <vector>

namespace v4r
{
      namespace io
      {

          /** Returns folder names in a folder </br>
          * @param dir
          * @param rel_path_so_far
          * @param relative_paths
          * @return number of folders in folder
          */
          V4R_EXPORTS int
          getFoldersInDirectory (const std::string & dir,
                                 const std::string & rel_path_so_far,
                                 std::vector<std::string> & relative_paths);



        /** Returns a the name of files in a folder </br>
        * '(.*)bmp'
        * @param dir
        * @param relative_paths
        * @param rel_path_so_far
        * @param regex_pattern examples "(.*)bmp",  "(.*)$"
        * @param recursive (true if files in subfolders should be returned as well)
        * @return number of files in folder (-1 in case directory name is not valid)
        */
        V4R_EXPORTS int
        getFilesInDirectory (   const std::string & dir,
                                std::vector<std::string> & relative_paths,
                                const std::string & rel_path_so_far = std::string(""),
                                const std::string & regex_pattern = std::string(""),
                                bool recursive = true);

        /** checks if a file exists
        * @param rFile
        * @return true if file exsits
        */
        V4R_EXPORTS bool
        existsFile ( const std::string &rFile );

        /** checks if a folder exists
        * @param rFolder
        * @return true if folder exsits
        */
        V4R_EXPORTS bool
        existsFolder ( const std::string &rFolder );

        /** checks if folder already exists and if not, creates one
          * @param folder_name
          */
        V4R_EXPORTS void
        createDirIfNotExist(const std::string & dirs);

        /** checks if the path for the filename already exists,
         * otherwise creates it
         * @param filename
         */
        V4R_EXPORTS void
        createDirForFileIfNotExist(const std::string & filename);

      }

}

#endif /* V4R_IO_FILESYSTEM_H_ */
