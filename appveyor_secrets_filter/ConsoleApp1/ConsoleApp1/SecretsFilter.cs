// Decompiled with JetBrains decompiler
// Type: Appveyor.BuildAgent.Service.SecretsFilter
// Assembly: appveyor-build-agent, Version=7.0.3212.0, Culture=neutral, PublicKeyToken=null
// MVID: 106B3391-E7B3-4D4F-BFB0-6A855B2383E5
// Assembly location: C:\Users\daniel\Downloads\HostAgent\appveyor-build-agent.dll

#nullable disable
using System.Text;

namespace ConsoleApp1
{
  public class SecretsFilter
  {
    public const int BUFFER_LENGTH = 65535;
    private IList<SecretsFilter.SecretWord> _secretWords;
    private StringBuilder _output = new StringBuilder();
    private char[] _buffer = new char[(int) ushort.MaxValue];
    private SecretsFilter.MatchResult _lastMatch;
    private int i;
    private int mStart = -1;
    private int mEnd = -1;

    public SecretsFilter(IEnumerable<string> secrets)
    {
      this._secretWords = (IList<SecretsFilter.SecretWord>) secrets.Select<string, SecretsFilter.SecretWord>((Func<string, SecretsFilter.SecretWord>) (s => new SecretsFilter.SecretWord(s))).ToList<SecretsFilter.SecretWord>();
    }

    public string Filter(string input)
    {
      if (string.IsNullOrEmpty(input))
      {
        if (this._lastMatch > SecretsFilter.MatchResult.NoMatch)
          this.CopyBuffer();
        string str = this._output.ToString();
        this._output.Clear();
        return str;
      }
      foreach (char ch in input)
      {
        this._buffer[this.i] = ch;
        this._lastMatch = SecretsFilter.MatchResult.NoMatch;
        foreach (SecretsFilter.SecretWord secretWord in (IEnumerable<SecretsFilter.SecretWord>) this._secretWords)
        {
          SecretsFilter.MatchResult matchResult = secretWord.Match(ch);
          this._lastMatch |= matchResult;
          if (matchResult == SecretsFilter.MatchResult.FullMatch)
          {
            this.mEnd = this.i;
            int num = this.i - secretWord.Length + 1;
            if (num < this.mStart || this.mStart == -1)
              this.mStart = num;
          }
        }
        if (this._lastMatch == SecretsFilter.MatchResult.NoMatch)
          this.CopyBuffer();
        else
          ++this.i;
      }
      if ((this._lastMatch & SecretsFilter.MatchResult.PartialMatch) > SecretsFilter.MatchResult.NoMatch)
        return (string) null;
      string str1 = this._output.ToString();
      this._output.Clear();
      return str1;
    }

    private void CopyBuffer()
    {
      if (this.mStart == -1 && this.mEnd == -1)
      {
        this._output.Append(this._buffer, 0, this.i + 1);
      }
      else
      {
        if (this.mStart > 0)
          this._output.Append(this._buffer, 0, this.mStart);
        this._output.Append("***");
        if (this.mEnd > -1)
          this._output.Append(this._buffer, this.mEnd + 1, this.i - this.mEnd);
      }
      this.i = 0;
      this.mStart = -1;
      this.mEnd = -1;
    }

    [Flags]
    public enum MatchResult
    {
      NoMatch = 0,
      PartialMatch = 1,
      FullMatch = 2,
    }

    public class SecretWord
    {
      private string _data;
      private int i;

      public int Length => this._data.Length;

      public SecretWord(string data) => this._data = data;

      public SecretsFilter.MatchResult Match(char ch)
      {
        if (this.i == this._data.Length)
          this.i = 0;
        bool flag = (int) this._data[this.i] == (int) ch;
        if (flag && this.i == this._data.Length - 1)
          return SecretsFilter.MatchResult.FullMatch;
        if (!flag && this.i > 0)
        {
          this.i = 0;
          flag = (int) this._data[this.i] == (int) ch;
        }
        if (!flag)
          return SecretsFilter.MatchResult.NoMatch;
        ++this.i;
        return SecretsFilter.MatchResult.PartialMatch;
      }
    }
  }
}
