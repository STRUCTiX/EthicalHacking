// See https://aka.ms/new-console-template for more information

using System.Collections;
using ConsoleApp1;

List<string> abc =
[
    "aba123",

    "-----BEGIN OPENSSH PRIVATE KEY-----b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAMwAAAAtzc2gtZWQyNTUxOQAAACDLPL7p6n8ZH6ouA7R1FzsEbU1LKvKkcxfPhvL6v5AnagAAAJgDs3ZhA7N2YQAAAAtzc2gtZWQyNTUxOQAAACDLPL7p6n8ZH6ouA7R1FzsEbU1LKvKkcxfPhvL6v5AnagAAAEAMHETQH3lqjxCy4Pw0pd0vub/mDaR0u5IX8HZ9VYsUucs8vunqfxkfqi4DtHUXOwRtTUsq8qRzF8+G8vq/kCdqAAAAE2FwcHZleW9yQEFQUFZZUi1XSU4BAg==-----END OPENSSH PRIVATE KEY-----"
];
SecretsFilter sf = new SecretsFilter(abc);


Console.WriteLine(sf.Filter("-----BEGIN OPENSSH PRIVATE KEY-----"));
Console.WriteLine(sf.Filter("b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAMwAAAAtzc2gtZW"));
Console.WriteLine(sf.Filter("QyNTUxOQAAACDLPL7p6n8ZH6ouA7R1FzsEbU1LKvKkcxfPhvL6v5AnagAAAJgDs3ZhA7N2"));
Console.WriteLine(sf.Filter("YQAAAAtzc2gtZWQyNTUxOQAAACDLPL7p6n8ZH6ouA7R1FzsEbU1LKvKkcxfPhvL6v5Anag"));
Console.WriteLine(sf.Filter("AAAEAMHETQH3lqjxCy4Pw0pd0vub/mDaR0u5IX8HZ9VYsUucs8vunqfxkfqi4DtHUXOwRt"));
Console.WriteLine(sf.Filter("TUsq8qRzF8+G8vq/kCdqAAAAE2FwcHZleW9yQEFQUFZZUi1XSU4BAg=="));
Console.WriteLine(sf.Filter("-----END OPENSSH PRIVATE KEY-----"));

Console.WriteLine("1" + sf.Filter("ababa123"));

Console.WriteLine("2" + sf.Filter("x------BEGIN OPENSSH PRIVATE KEY-----"));
Console.WriteLine("3" + sf.Filter("x-----BEGIN OPENSSH PRIVATE KEY-----x"));