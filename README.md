
	flush some unneeded code
	fixed and improved for parsing C++ class members/functions from .pdb
	just dump pdb
	all ref from upstream

improved:

* show typedef types inside struct/class-es
* show static member of struct/class-es
* show functions with args and name of args and return type
* hide return type for ctor/dtor
* show access like public/private/protected for members of class-es
* show offset for virtual function, note there is need todo for more level of derive
* show derive from struct/class-es
* drop PADDING at end of struct/class-es
* show global datas type like static, functions or else
