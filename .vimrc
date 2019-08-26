augroup project
  autocmd!
  autocmd BufRead,BufNewFile *.h,*.c set filetype=c.doxygen
augroup END

let &path.="src/include,/usr/include/,"

set makeprg=compiledb\ make
nnoremap <F4> :make!<cr><cr>
nnoremap <F5> :!reset; sudo ./main --timeout 300 --ports 80,443,22
nnoremap <F6> :!reset; sudo gdb --args ./main --timeout 300 --ports 80,443,22
