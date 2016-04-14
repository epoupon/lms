/*
 * Copyright (c) 2005, Seweryn Habdank-Wojewodzki
 *
 * All rights reserved.
 *
 * Permission to copy, use, modify, sell and distribute this software
 * is granted provided this copyright notice appears in all copies.
 * This software is provided "as is" without express or implied
 * warranty, and with no claim as to its suitability for any purpose.
 * This software has generative abilities, but does not claim any rights
 * nor guarantees with regard to the generated code.
 */

/*
 * e-mail: habdank{AT}gmail{dot}com
 *
 * File created: Sat 05 Nov 2005 14:20:28 CET
 * Last modified: Wed 08 Aug 2007 17:16:20 CEST
 */

/**
 * \defgroup trivial_debugger Trivial Debugger
 */

/**
 * \file debugger.hpp
 * \ingroup trivial_debugger
 * \brief Defines macro for Trivial Debugger.
 * When compiler send to program one of flags: TDEBUG, ETDEBUG or FTDEBUG debugger functionality is set.
 */

/**
 * \def D
 * \brief Macro D will send to proper stream value of the variable.
 * Stream depends on set flag. Information which is send looks like:
 *
 * __FILE__ [__LINE__] : variable name = variable value
 *
 * \param name is name of debugged variable.
 * \par Usage:
 *
 * In the main file e.g. main.cpp in global scope should be placed:
 *
 * #ifdef FTDEBUG
 * ::std::auto_ptr < ::std::ofstream > DEBUGGER_STREAM;
 * #endif //FTDEGUG
 *
 * And at the very beginnig of the main function should be placed:
 *
 * #ifdef FTDEBUG
 *     DEBUGGER_STREAM =
 *         ::std::auto_ptr < ::std::ofstream >
 *         ( new ::std::ofstream ( "_debugger.out" ) );
 * #endif //FTDEBUG
 *
 * if we suppose to use file to make logs.
 *
 * And in the application we have to include this header and just use macro:
 *
 * Available for all cases including compilation
 * without any flag:
 *
 * D ( my_variable_name ); or D ( "I am here." );
 *
 * If are sure that debugger will work by default any of flag TDEBUG, ETDEBUG and FTDEBUG is set:
 *
 * D ( ) << "Send other information to the stream." << ::std::endl;
 *
 * Or just work on if we are sure about existence of the log file FTDEBUG is set:
 *
 * *DEBUGGER_STREAM << any_function_that_returns_reference_to_ostream ( ) << ::std::endl;
 *
 * \par Remark:
 * Remember to put semicolon at the end!
 *
 * \par Flags:
 *
 * - TDEBUG - Trivial Debugger sends debugging information to the stdout.
 *
 * - ETDEBUG - Trivial Debugger sends debugging information to the stderr.
 *
 * - FTDEBUG - Trivial Debugger sends debugging information to the file
 * (generally declared and defined in main() function of the program).
 */

#ifndef DEBUGGER_HPP_INCLUDED
#define DEBUGGER_HPP_INCLUDED

#ifdef FTDEBUG

    #include <memory>
    #include <fstream>

    #define D(name) *DEBUGGER_STREAM << __FILE__ << " [" << __LINE__ << "] : " << #name << " = " << (name) << ::std::endl
    extern ::std::auto_ptr < ::std::ofstream > DEBUGGER_STREAM;

#elif defined(TDEBUG)

    #include <iostream>
    #define D(name) ::std::cout << __FILE__ << " [" << __LINE__ << "] : " << #name << " = " << (name) << ::std::endl

#elif defined(ETDEBUG)

    #include <iostream>
    #define D(name) ::std::cerr << __FILE__ << " [" << __LINE__ << "] : " << #name << " = " << (name) << ::std::endl

#else

    #define D(name) {}

#endif // ..TDEBUG

#endif // DEBUGGER_HPP_INCLUDED

